#include <iostream>
#include <random>

#include <legion.h>

#include "COOMatrix.hpp"
#include "ConjugateGradientSolver.hpp"
#include "Tasks.hpp"

enum TaskIDs : Legion::TaskID {
    TOP_LEVEL_TASK_ID = 10,
    FILL_COO_MATRIX_TASK_ID = 11,
    FILL_VECTOR_TASK_ID = 12,
    PRINT_TASK_ID = 13,
    PRINT_VEC_TASK_ID = 16,
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

constexpr Legion::coord_t MATRIX_SIZE = 16;
constexpr Legion::coord_t NUM_NONZERO_ENTRIES = 3 * MATRIX_SIZE - 2;
constexpr Legion::coord_t NUM_INPUT_PARTITIONS = 4;
constexpr Legion::coord_t NUM_OUTPUT_PARTITIONS = 4;

Legion::LogicalRegion create_region(
    Legion::coord_t size,
    const std::vector<std::pair<std::size_t, Legion::FieldID>> &fields,
    Legion::Context ctx, Legion::Runtime *rt) {
    Legion::IndexSpace index_space =
        rt->create_index_space(ctx, Legion::Rect<1>{0, size - 1});
    Legion::FieldSpace field_space = rt->create_field_space(ctx);
    Legion::FieldAllocator allocator =
        rt->create_field_allocator(ctx, field_space);
    for (const auto [field_size, field_id] : fields) {
        allocator.allocate_field(field_size, field_id);
    }
    return rt->create_logical_region(ctx, index_space, field_space);
}

Legion::LogicalRegion create_region(
    Legion::IndexSpace index_space,
    const std::vector<std::pair<std::size_t, Legion::FieldID>> &fields,
    Legion::Context ctx, Legion::Runtime *rt) {
    Legion::FieldSpace field_space = rt->create_field_space(ctx);
    Legion::FieldAllocator allocator =
        rt->create_field_allocator(ctx, field_space);
    for (const auto [field_size, field_id] : fields) {
        allocator.allocate_field(field_size, field_id);
    }
    return rt->create_logical_region(ctx, index_space, field_space);
}

void top_level_task(const Legion::Task *,
                    const std::vector<Legion::PhysicalRegion> &,
                    Legion::Context ctx, Legion::Runtime *rt) {

    const auto coo_matrix = create_region(NUM_NONZERO_ENTRIES,
                                          {{sizeof(Legion::coord_t), FID_COO_I},
                                           {sizeof(Legion::coord_t), FID_COO_J},
                                           {sizeof(double), FID_COO_ENTRY}},
                                          ctx, rt);

    const Legion::IndexSpace index_space =
        rt->create_index_space(ctx, Legion::Rect<1>{0, MATRIX_SIZE - 1});
    const auto input_vector =
        create_region(index_space, {{sizeof(double), FID_VEC_ENTRY}}, ctx, rt);
    const auto output_vector =
        create_region(index_space, {{sizeof(double), FID_VEC_ENTRY}}, ctx, rt);

    auto input_color_space = rt->create_index_space(
        ctx, Legion::Rect<1>{0, NUM_INPUT_PARTITIONS - 1});
    auto output_color_space = rt->create_index_space(
        ctx, Legion::Rect<1>{0, NUM_OUTPUT_PARTITIONS - 1});

    auto input_partition = rt->create_equal_partition(
        ctx, input_vector.get_index_space(), input_color_space);
    auto output_partition = rt->create_equal_partition(
        ctx, output_vector.get_index_space(), output_color_space);

    {
        Legion::TaskLauncher launcher{FILL_COO_MATRIX_TASK_ID,
                                      Legion::TaskArgument{nullptr, 0}};
        launcher.add_region_requirement(Legion::RegionRequirement{
            coo_matrix, WRITE_DISCARD, EXCLUSIVE, coo_matrix});
        launcher.add_field(0, FID_COO_I);
        launcher.add_field(0, FID_COO_J);
        launcher.add_field(0, FID_COO_ENTRY);
        rt->execute_task(ctx, launcher);
    }

    {
        Legion::TaskLauncher launcher{FILL_VECTOR_TASK_ID,
                                      Legion::TaskArgument{nullptr, 0}};
        launcher.add_region_requirement(Legion::RegionRequirement{
            input_vector, WRITE_DISCARD, EXCLUSIVE, input_vector});
        launcher.add_field(0, FID_VEC_ENTRY);
        rt->execute_task(ctx, launcher);
    }

    COOMatrix matrix_obj{
        coo_matrix,      FID_COO_I,        FID_COO_J, FID_COO_ENTRY,
        input_partition, output_partition, ctx,       rt};

    matrix_obj.launch_matmul(output_vector, FID_VEC_ENTRY, input_vector,
                             FID_VEC_ENTRY, ctx, rt);

    {
        Legion::TaskLauncher launcher{PRINT_VEC_TASK_ID,
                                      Legion::TaskArgument{nullptr, 0}};
        launcher.add_region_requirement(Legion::RegionRequirement{
            output_vector, READ_ONLY, EXCLUSIVE, output_vector});
        launcher.add_field(0, FID_VEC_ENTRY);
        rt->execute_task(ctx, launcher);
    }

    // {
    //     Legion::TaskLauncher launcher{BOUNDARY_FILL_VECTOR_TASK_ID,
    //                                   Legion::TaskArgument{nullptr, 0}};
    //     launcher.add_region_requirement(Legion::RegionRequirement{
    //         output_vector, READ_ONLY, EXCLUSIVE, output_vector});
    //     launcher.add_field(0, FID_VEC_ENTRY);
    //     rt->execute_task(ctx, launcher);
    // }

    // {
    //     Legion::TaskLauncher launcher{PRINT_VEC_TASK_ID,
    //                                   Legion::TaskArgument{nullptr, 0}};
    //     launcher.add_region_requirement(Legion::RegionRequirement{
    //         output_vector, READ_ONLY, EXCLUSIVE, output_vector});
    //     launcher.add_field(0, FID_VEC_ENTRY);
    //     rt->execute_task(ctx, launcher);
    // }

    ConjugateGradientSolver solver{output_vector, FID_VEC_ENTRY, ctx, rt};

    { // x = 0
        Legion::TaskLauncher launcher{ZERO_FILL_TASK_ID,
                                      Legion::TaskArgument{nullptr, 0}};
        launcher.add_region_requirement(Legion::RegionRequirement{
            solver.workspace, WRITE_DISCARD, EXCLUSIVE, solver.workspace});
        launcher.add_field(0, FID_CG_X);
        rt->execute_task(ctx, launcher);
    }

    { // p = b
        Legion::TaskLauncher launcher{COPY_TASK_ID,
                                      Legion::TaskArgument{nullptr, 0}};
        launcher.add_region_requirement(Legion::RegionRequirement{
            solver.workspace, WRITE_DISCARD, EXCLUSIVE, solver.workspace});
        launcher.add_field(0, FID_CG_P);
        launcher.add_region_requirement(Legion::RegionRequirement{
            output_vector, READ_ONLY, EXCLUSIVE, output_vector});
        launcher.add_field(1, FID_VEC_ENTRY);
        rt->execute_task(ctx, launcher);
    }

    { // r = b
        Legion::TaskLauncher launcher{COPY_TASK_ID,
                                      Legion::TaskArgument{nullptr, 0}};
        launcher.add_region_requirement(Legion::RegionRequirement{
            solver.workspace, WRITE_DISCARD, EXCLUSIVE, solver.workspace});
        launcher.add_field(0, FID_CG_R);
        launcher.add_region_requirement(Legion::RegionRequirement{
            output_vector, READ_ONLY, EXCLUSIVE, output_vector});
        launcher.add_field(1, FID_VEC_ENTRY);
        rt->execute_task(ctx, launcher);
    }

    Legion::Future r_norm2 = solver.dot_product(FID_CG_R, FID_CG_R, ctx, rt);

    for (int i = 0; i < MATRIX_SIZE; ++i) {
        matrix_obj.launch_matmul(solver.workspace, FID_CG_Q, solver.workspace,
                                 FID_CG_P, ctx, rt);
        Legion::Future p_norm = solver.dot_product(FID_CG_P, FID_CG_Q, ctx, rt);
        Legion::Future alpha = solver.divide(r_norm2, p_norm, ctx, rt);
        solver.axpy(FID_CG_X, alpha, FID_CG_P, ctx, rt);
        solver.axpy(FID_CG_R, solver.negate(alpha, ctx, rt), FID_CG_Q, ctx, rt);
        Legion::Future r_norm2_new =
            solver.dot_product(FID_CG_R, FID_CG_R, ctx, rt);
        std::cout << r_norm2_new.get_result<double>() << std::endl;
        Legion::Future beta = solver.divide(r_norm2_new, r_norm2, ctx, rt);
        r_norm2 = r_norm2_new;
        solver.xpay(FID_CG_P, beta, FID_CG_R, ctx, rt);
    }

    {
        Legion::TaskLauncher launcher{PRINT_VEC_TASK_ID,
                                      Legion::TaskArgument{nullptr, 0}};
        launcher.add_region_requirement(Legion::RegionRequirement{
            solver.workspace, READ_ONLY, EXCLUSIVE, solver.workspace});
        launcher.add_field(0, FID_CG_X);
        rt->execute_task(ctx, launcher);
    }
}

void fill_coo_matrix_task(const Legion::Task *task,
                          const std::vector<Legion::PhysicalRegion> &regions,
                          Legion::Context ctx, Legion::Runtime *rt) {

    assert(regions.size() == 1);
    const auto &coo_matrix = regions[0];

    const Legion::FieldAccessor<WRITE_DISCARD, Legion::coord_t, 1> i_writer{
        coo_matrix, FID_COO_I};
    const Legion::FieldAccessor<WRITE_DISCARD, Legion::coord_t, 1> j_writer{
        coo_matrix, FID_COO_J};
    const Legion::FieldAccessor<WRITE_DISCARD, double, 1> entry_writer{
        coo_matrix, FID_COO_ENTRY};

    if (NUM_NONZERO_ENTRIES == 3 * MATRIX_SIZE - 2) {
        Legion::PointInDomainIterator<1> iter{coo_matrix};
        for (Legion::coord_t i = 0; i < MATRIX_SIZE; ++i) {
            i_writer[*iter] = i;
            j_writer[*iter] = i;
            entry_writer[*iter] = 2.0;
            ++iter;
        }
        for (Legion::coord_t i = 0; i < MATRIX_SIZE - 1; ++i) {
            i_writer[*iter] = i + 1;
            j_writer[*iter] = i;
            entry_writer[*iter] = -1.0;
            ++iter;
            i_writer[*iter] = i;
            j_writer[*iter] = i + 1;
            entry_writer[*iter] = -1.0;
            ++iter;
        }
    } else {
        std::set<std::pair<Legion::coord_t, Legion::coord_t>> indices{};
        std::random_device rng{};
        std::uniform_int_distribution<Legion::coord_t> index_dist{
            0, MATRIX_SIZE - 1};
        std::uniform_real_distribution<double> entry_dist{0.0, 1.0};
        while (indices.size() < NUM_NONZERO_ENTRIES) {
            indices.emplace(index_dist(rng), index_dist(rng));
        }
        for (Legion::PointInDomainIterator<1> iter{coo_matrix}; iter();
             ++iter) {
            const auto [i, j] = *indices.begin();
            indices.erase(indices.begin());
            const double entry = entry_dist(rng);
            i_writer[*iter] = i;
            j_writer[*iter] = j;
            entry_writer[*iter] = entry;
            std::cout << *iter << ": " << i << ", " << j << ", " << entry
                      << std::endl;
        }
    }
}

void fill_vector_task(const Legion::Task *task,
                      const std::vector<Legion::PhysicalRegion> &regions,
                      Legion::Context ctx, Legion::Runtime *rt) {
    assert(regions.size() == 1);
    const auto &vector = regions[0];
    const Legion::FieldAccessor<WRITE_DISCARD, double, 1> entry_writer{
        vector, FID_VEC_ENTRY};
    std::random_device rng{};
    std::uniform_real_distribution<double> entry_dist{0.0, 1.0};
    for (Legion::PointInDomainIterator<1> iter{vector}; iter(); ++iter) {
        const double entry = entry_dist(rng);
        entry_writer[*iter] = entry;
        std::cout << *iter << ": " << entry << std::endl;
    }
}

void boundary_fill_vector_task(
    const Legion::Task *task,
    const std::vector<Legion::PhysicalRegion> &regions, Legion::Context ctx,
    Legion::Runtime *rt) {
    assert(regions.size() == 1);
    const auto &vector = regions[0];
    const Legion::FieldAccessor<WRITE_DISCARD, double, 1> entry_writer{
        vector, FID_VEC_ENTRY};
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

void print_task(const Legion::Task *task,
                const std::vector<Legion::PhysicalRegion> &regions,
                Legion::Context ctx, Legion::Runtime *rt) {
    assert(regions.size() == 1);
    const auto &coo_matrix = regions[0];
    const Legion::FieldAccessor<READ_ONLY, Legion::coord_t, 1> i_reader{
        coo_matrix, FID_COO_I};
    const Legion::FieldAccessor<READ_ONLY, Legion::coord_t, 1> j_reader{
        coo_matrix, FID_COO_J};
    const Legion::FieldAccessor<READ_ONLY, double, 1> entry_reader{
        coo_matrix, FID_COO_ENTRY};
    std::cout << task->index_point << std::endl;
    for (Legion::PointInDomainIterator<1> iter{coo_matrix}; iter(); ++iter) {
        std::cout << task->index_point << *iter << ": " << i_reader[*iter]
                  << ", " << j_reader[*iter] << ", " << entry_reader[*iter]
                  << std::endl;
    }
}

void print_vec_task(const Legion::Task *task,
                    const std::vector<Legion::PhysicalRegion> &regions,
                    Legion::Context ctx, Legion::Runtime *rt) {

    assert(regions.size() == 1);
    const auto &vector = regions[0];

    assert(task->regions.size() == 1);
    const auto &vector_req = task->regions[0];

    assert(vector_req.privilege_fields.size() == 1);
    const Legion::FieldID vector_fid = *vector_req.privilege_fields.begin();

    const Legion::FieldAccessor<READ_ONLY, double, 1> entry_reader{vector,
                                                                   vector_fid};

    for (Legion::PointInDomainIterator<1> iter{vector}; iter(); ++iter) {
        std::cout << task->index_point << ' ' << *iter << ": "
                  << entry_reader[*iter] << std::endl;
    }
}

int main(int argc, char **argv) {
    preregister_solver_tasks<double, 1>();
    preregister_cpu_task<top_level_task>(TOP_LEVEL_TASK_ID, "top_level");
    preregister_cpu_task<fill_coo_matrix_task>(FILL_COO_MATRIX_TASK_ID,
                                               "fill_coo_matrix");
    preregister_cpu_task<fill_vector_task>(FILL_VECTOR_TASK_ID, "fill_vector");
    preregister_cpu_task<print_task>(PRINT_TASK_ID, "print");
    preregister_cpu_task<print_vec_task>(PRINT_VEC_TASK_ID, "print_vec_task");
    preregister_cpu_task<boundary_fill_vector_task>(
        BOUNDARY_FILL_VECTOR_TASK_ID, "boundarey_fill");

    Legion::Runtime::set_top_level_task_id(TOP_LEVEL_TASK_ID);
    return Legion::Runtime::start(argc, argv);
}
