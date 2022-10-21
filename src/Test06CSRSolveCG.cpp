#include <cassert>

#include <legion.h>
#include <realm/cmdline.h>

#include "CGSolver.hpp"
#include "CSRMatrix.hpp"
#include "ExampleSystems.hpp"
#include "Initialize.hpp"
#include "LegionUtilities.hpp"
#include "LibraryOptions.hpp"
#include "PartitionedVector.hpp"
#include "SquarePlanner.hpp"

enum TaskIDs : Legion::TaskID { TOP_LEVEL_TASK_ID };

using ENTRY_T = double; // vary this
constexpr int VECTOR_DIM = 1;
constexpr int KERNEL_DIM = 1;
constexpr int DOMAIN_DIM = 1;
constexpr int RANGE_DIM = 1;
using VECTOR_COORD_T = Legion::coord_t; // TODO: can't vary this yet
using KERNEL_COORD_T = Legion::coord_t; // TODO: can't vary this yet
using DOMAIN_COORD_T = Legion::coord_t; // TODO: can't vary this yet
using RANGE_COORD_T = Legion::coord_t;  // TODO: can't vary this yet
using VectorRect = Legion::Rect<VECTOR_DIM, VECTOR_COORD_T>;
using KernelRect = Legion::Rect<KERNEL_DIM, KERNEL_COORD_T>;

constexpr Legion::FieldID FID_COL = 0;
constexpr Legion::FieldID FID_ENTRY = 1;
constexpr Legion::FieldID FID_ROWPTR = 2;


void top_level_task(
    const Legion::Task *,
    const std::vector<Legion::PhysicalRegion> &,
    Legion::Context ctx,
    Legion::Runtime *rt
) {

    constexpr int VECTOR_COLOR_DIM = 1;           // TODO: vary this!
    using VECTOR_COLOR_COORD_T = Legion::coord_t; // TODO: vary this!
    using VectorColorRect =
        Legion::Rect<VECTOR_COLOR_DIM, VECTOR_COLOR_COORD_T>;

    VECTOR_COORD_T grid_size = 100;
    VECTOR_COLOR_COORD_T num_vector_pieces = 4;
    std::size_t num_iterations = 10;
    bool no_print_results = false;

    const Legion::InputArgs &args = Legion::Runtime::get_input_args();
    bool ok = Realm::CommandLineParser()
                  .add_option_int("-n", grid_size)
                  .add_option_int("-vp", num_vector_pieces)
                  .add_option_int("-it", num_iterations)
                  .add_option_bool("-np", no_print_results)
                  .parse_command_line(args.argc, (const char **) args.argv);
    assert(ok);

    const auto vector_index_space =
        rt->create_index_space(ctx, VectorRect{0, grid_size - 1});

    const auto vector_color_space =
        rt->create_index_space(ctx, VectorColorRect{0, num_vector_pieces - 1});

    const KERNEL_COORD_T kernel_size = LegionSolvers::laplacian_1d_kernel_size(
        static_cast<KERNEL_COORD_T>(grid_size)
    );

    const auto matrix_index_space =
        rt->create_index_space(ctx, KernelRect{0, kernel_size - 1});

    const auto matrix_field_space = LegionSolvers::create_field_space(
        ctx,
        rt,
        {sizeof(ENTRY_T), sizeof(Legion::Point<VECTOR_DIM, VECTOR_COORD_T>)},
        {FID_ENTRY, FID_COL}
    );

    const auto rowptr_field_space = LegionSolvers::create_field_space(
        ctx,
        rt,
        {sizeof(Legion::Rect<VECTOR_DIM, VECTOR_COORD_T>)},
        {FID_ROWPTR}
    );

    {
        const Legion::IndexPartitionT<VECTOR_DIM, VECTOR_COORD_T>
            disjoint_vector_partition = rt->create_equal_partition(
                ctx, vector_index_space, vector_color_space
            );

        const auto matrix_region = rt->create_logical_region(
            ctx, matrix_index_space, matrix_field_space
        );

        const auto rowptr_region = rt->create_logical_region(
            ctx, vector_index_space, rowptr_field_space
        );

        {
            const typename LegionSolvers::FillCSRNegativeLaplacianTask<
                ENTRY_T,
                KERNEL_DIM,
                DOMAIN_DIM,
                RANGE_DIM,
                KERNEL_COORD_T,
                DOMAIN_COORD_T,
                RANGE_COORD_T>::Args args{FID_COL, FID_ENTRY, grid_size};
            Legion::TaskLauncher launcher{
                LegionSolvers::FillCSRNegativeLaplacianTask<
                    ENTRY_T,
                    KERNEL_DIM,
                    DOMAIN_DIM,
                    RANGE_DIM,
                    KERNEL_COORD_T,
                    DOMAIN_COORD_T,
                    RANGE_COORD_T>::task_id,
                Legion::TaskArgument{&args, sizeof(args)}};
            launcher.map_id = LegionSolvers::LEGION_SOLVERS_MAPPER_ID;
            launcher.add_region_requirement(Legion::RegionRequirement{
                matrix_region,
                LEGION_WRITE_DISCARD,
                LEGION_EXCLUSIVE,
                matrix_region});
            launcher.add_field(0, FID_COL);
            launcher.add_field(0, FID_ENTRY);
            rt->execute_task(ctx, launcher);
        }

        {
            const typename LegionSolvers::FillCSRNegativeLaplacianRowptrTask<
                ENTRY_T,
                KERNEL_DIM,
                DOMAIN_DIM,
                RANGE_DIM,
                KERNEL_COORD_T,
                DOMAIN_COORD_T,
                RANGE_COORD_T>::Args args{FID_ROWPTR, grid_size};
            Legion::TaskLauncher launcher{
                LegionSolvers::FillCSRNegativeLaplacianRowptrTask<
                    ENTRY_T,
                    KERNEL_DIM,
                    DOMAIN_DIM,
                    RANGE_DIM,
                    KERNEL_COORD_T,
                    DOMAIN_COORD_T,
                    RANGE_COORD_T>::task_id,
                Legion::TaskArgument{&args, sizeof(args)}};
            launcher.map_id = LegionSolvers::LEGION_SOLVERS_MAPPER_ID;
            launcher.add_region_requirement(Legion::RegionRequirement{
                rowptr_region,
                LEGION_WRITE_DISCARD,
                LEGION_EXCLUSIVE,
                rowptr_region});
            launcher.add_field(0, FID_ROWPTR);
            rt->execute_task(ctx, launcher);
        }

        LegionSolvers::CSRMatrix<ENTRY_T> csr_matrix(
            ctx,
            rt,
            matrix_region,
            FID_COL,
            FID_ENTRY,
            rowptr_region,
            FID_ROWPTR
        );

        {
            LegionSolvers::PartitionedVector<ENTRY_T> rhs{
                ctx, rt, "rhs", disjoint_vector_partition};
            LegionSolvers::PartitionedVector<ENTRY_T> sol{
                ctx, rt, "sol", disjoint_vector_partition};

            rhs.constant_fill(1.0);
            sol.zero_fill();

            LegionSolvers::SquarePlanner<ENTRY_T> planner{ctx, rt};
            planner.add_sol_vector(sol);
            planner.add_rhs_vector(rhs);
            planner.add_row_partitioned_matrix(csr_matrix, 0, 0);

            LegionSolvers::CGSolver<ENTRY_T> solver{planner};

            for (std::size_t i = 0; i < num_iterations; ++i) {
                rt->begin_trace(ctx, 51);
                solver.step();
                rt->end_trace(ctx, 51);
            }

            if (!no_print_results) {
                Legion::Future dummy = Legion::Future::from_value<int>(rt, 0);
                for (std::size_t i = 0; i <= num_iterations; ++i) {
                    dummy = solver.residual_norm_squared[i].print(dummy);
                }
            }
        }

        rt->destroy_index_partition(ctx, disjoint_vector_partition);
        rt->destroy_logical_region(ctx, matrix_region);
        rt->destroy_logical_region(ctx, rowptr_region);
    }

    rt->destroy_index_space(ctx, vector_index_space);
    rt->destroy_index_space(ctx, vector_color_space);
    rt->destroy_index_space(ctx, matrix_index_space);
    rt->destroy_field_space(ctx, matrix_field_space);
}


int main(int argc, char **argv) {
    using LegionSolvers::TaskFlags;
    LegionSolvers::initialize(false);
    LegionSolvers::preregister_task<top_level_task>(
        TOP_LEVEL_TASK_ID,
        "top_level",
        TaskFlags::REPLICABLE | TaskFlags::INNER,
        Legion::Processor::LOC_PROC
    );
    Legion::Runtime::set_top_level_task_id(TOP_LEVEL_TASK_ID);
    return Legion::Runtime::start(argc, argv);
}