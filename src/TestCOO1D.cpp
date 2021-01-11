#include <iostream>
#include <random>

#include <legion.h>

#include "COOMatrix.hpp"
#include "ConjugateGradientSolver.hpp"
#include "ExampleSystems.hpp"
#include "Planner.hpp"
#include "TaskRegistration.hpp"

using LegionSolvers::ConjugateGradientSolver;
using LegionSolvers::COOMatrix;
using LegionSolvers::Planner;

constexpr Legion::coord_t MATRIX_SIZE = 16;
constexpr Legion::coord_t NUM_NONZERO_ENTRIES = 3 * MATRIX_SIZE - 2;
constexpr Legion::coord_t NUM_INPUT_PARTITIONS = 4;
constexpr Legion::coord_t NUM_OUTPUT_PARTITIONS = 4;

enum TaskIDs : Legion::TaskID {
    TOP_LEVEL_TASK_ID = 10,
    FILL_VECTOR_TASK_ID = 12,
    BOUNDARY_FILL_VECTOR_TASK_ID = 17,
};


enum COOMatrixFieldIDs : Legion::FieldID {
    FID_COO_I = 101,
    FID_COO_J = 102,
    FID_COO_ENTRY = 103,
};


enum VectorFieldIDs : Legion::FieldID {
    FID_VEC_ENTRY = 200,
};


void top_level_task(const Legion::Task *,
                    const std::vector<Legion::PhysicalRegion> &,
                    Legion::Context ctx,
                    Legion::Runtime *rt) {

    // Create two vector regions (input and output).
    const Legion::IndexSpaceT<1> index_space = rt->create_index_space(ctx, Legion::Rect<1>{0, MATRIX_SIZE - 1});
    const Legion::LogicalRegionT<1> input_vector =
        LegionSolvers::create_region(index_space, {{sizeof(double), FID_VEC_ENTRY}}, ctx, rt);
    const Legion::LogicalRegionT<1> output_vector =
        LegionSolvers::create_region(index_space, {{sizeof(double), FID_VEC_ENTRY}}, ctx, rt);

    // Partition input and output vectors.
    const Legion::IndexSpaceT<1> input_color_space =
        rt->create_index_space(ctx, Legion::Rect<1>{0, NUM_INPUT_PARTITIONS - 1});
    const Legion::IndexSpaceT<1> output_color_space =
        rt->create_index_space(ctx, Legion::Rect<1>{0, NUM_OUTPUT_PARTITIONS - 1});
    const Legion::IndexPartitionT<1> input_partition =
        Legion::IndexPartitionT<1>{rt->create_equal_partition(ctx, input_vector.get_index_space(), input_color_space)};
    const Legion::IndexPartitionT<1> output_partition = Legion::IndexPartitionT<1>{
        rt->create_equal_partition(ctx, output_vector.get_index_space(), output_color_space)};

    { // Fill input vector entries.
        Legion::TaskLauncher launcher{FILL_VECTOR_TASK_ID, Legion::TaskArgument{nullptr, 0}};
        launcher.add_region_requirement(
            Legion::RegionRequirement{input_vector, LEGION_WRITE_DISCARD, LEGION_EXCLUSIVE, input_vector});
        launcher.add_field(0, FID_VEC_ENTRY);
        rt->execute_task(ctx, launcher);
    }

    // Create 1D Laplacian matrix.
    const auto coo_matrix =
        LegionSolvers::coo_negative_laplacian_1d<double>(FID_COO_I, FID_COO_J, FID_COO_ENTRY, MATRIX_SIZE, ctx, rt);

    // Construct map of nonzero tiles.
    COOMatrix<double, 1, 1, 1> matrix_obj{coo_matrix,      FID_COO_I,        FID_COO_J, FID_COO_ENTRY,
                                          input_partition, output_partition, ctx,       rt};

    // Launch matrix-vector multiplication tasks.
    matrix_obj.matvec(output_vector, FID_VEC_ENTRY, input_vector, FID_VEC_ENTRY, ctx, rt);

    LegionSolvers::print_vector<double>(output_vector, FID_VEC_ENTRY, "output_vector", ctx, rt);

    // Create another rhs vector and partition it.
    const Legion::IndexSpaceT<1> rhs_index_space = rt->create_index_space(ctx, Legion::Rect<1>{0, MATRIX_SIZE - 1});
    const auto rhs_vector = LegionSolvers::create_region(rhs_index_space, {{sizeof(double), FID_VEC_ENTRY}}, ctx, rt);
    const auto rhs_partition = rt->create_equal_partition(ctx, rhs_vector.get_index_space(), input_color_space);

    { // Fill new rhs vector.
        Legion::TaskLauncher launcher{BOUNDARY_FILL_VECTOR_TASK_ID, Legion::TaskArgument{nullptr, 0}};
        launcher.add_region_requirement(
            Legion::RegionRequirement{rhs_vector, LEGION_WRITE_DISCARD, LEGION_EXCLUSIVE, rhs_vector});
        launcher.add_field(0, FID_VEC_ENTRY);
        rt->execute_task(ctx, launcher);
    }

    Planner<double> planner{};
    planner.add_rhs(output_vector, FID_VEC_ENTRY, output_partition);
    planner.add_rhs(rhs_vector, FID_VEC_ENTRY, rhs_partition);
    planner.add_coo_matrix<1, 1, 1>(0, 0, coo_matrix, FID_COO_I, FID_COO_J, FID_COO_ENTRY, ctx, rt);
    planner.add_coo_matrix<1, 1, 1>(1, 1, coo_matrix, FID_COO_I, FID_COO_J, FID_COO_ENTRY, ctx, rt);

    ConjugateGradientSolver solver{planner, ctx, rt};
    solver.set_max_iterations(17);
    solver.solve(ctx, rt);

    LegionSolvers::print_vector<double>(solver.workspace[0], LegionSolvers::ConjugateGradientSolver<double>::FID_CG_X,
                                        "sol0", ctx, rt);

    LegionSolvers::print_vector<double>(solver.workspace[1], LegionSolvers::ConjugateGradientSolver<double>::FID_CG_X,
                                        "sol1", ctx, rt);
}

void fill_vector_task(const Legion::Task *task,
                      const std::vector<Legion::PhysicalRegion> &regions,
                      Legion::Context ctx,
                      Legion::Runtime *rt) {
    assert(regions.size() == 1);
    const auto &vector = regions[0];
    const Legion::FieldAccessor<LEGION_WRITE_DISCARD, double, 1> entry_writer{vector, FID_VEC_ENTRY};
    std::random_device rng{};
    std::uniform_real_distribution<double> entry_dist{0.0, 1.0};
    for (Legion::PointInDomainIterator<1> iter{vector}; iter(); ++iter) {
        const double entry = entry_dist(rng);
        entry_writer[*iter] = entry;
        std::cout << *iter << ": " << entry << std::endl;
    }
}

void boundary_fill_vector_task(const Legion::Task *task,
                               const std::vector<Legion::PhysicalRegion> &regions,
                               Legion::Context ctx,
                               Legion::Runtime *rt) {
    assert(regions.size() == 1);
    const auto &vector = regions[0];
    const Legion::FieldAccessor<LEGION_WRITE_DISCARD, double, 1> entry_writer{vector, FID_VEC_ENTRY};
    for (Legion::PointInDomainIterator<1> iter{vector}; iter(); ++iter) {
        auto i = *iter;
        if (i[0] == 0) {
            entry_writer[*iter] = -1.0;
        } else if (i[0] == MATRIX_SIZE - 1) {
            entry_writer[*iter] = 2.0;
        } else {
            entry_writer[*iter] = 0.0;
        }
    }
}


int main(int argc, char **argv) {
    LegionSolvers::preregister_solver_tasks();

    LegionSolvers::preregister_cpu_task<top_level_task>(TOP_LEVEL_TASK_ID, "top_level");
    LegionSolvers::preregister_cpu_task<fill_vector_task>(FILL_VECTOR_TASK_ID, "fill_vector");
    LegionSolvers::preregister_cpu_task<boundary_fill_vector_task>(BOUNDARY_FILL_VECTOR_TASK_ID, "boundary_fill");

    Legion::Runtime::set_top_level_task_id(TOP_LEVEL_TASK_ID);
    return Legion::Runtime::start(argc, argv);
}
