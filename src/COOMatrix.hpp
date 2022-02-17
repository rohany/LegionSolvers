#ifndef LEGION_SOLVERS_COO_MATRIX_HPP
#define LEGION_SOLVERS_COO_MATRIX_HPP

#include <legion.h>

#include "AbstractMatrix.hpp"
#include "COOMatrixTasks.hpp"
#include "DenseDistributedVector.hpp"
#include "LibraryOptions.hpp"


namespace LegionSolvers {


    template <typename ENTRY_T>
    class COOMatrix : public AbstractMatrix<ENTRY_T> {

        Legion::Context ctx;
        Legion::Runtime *rt;
        Legion::IndexSpace kernel_space;
        Legion::LogicalRegion kernel_region;
        Legion::FieldID fid_i;
        Legion::FieldID fid_j;
        Legion::FieldID fid_entry;

    public:

        explicit COOMatrix(
            Legion::Context ctx,
            Legion::Runtime *rt,
            Legion::LogicalRegion kernel_region,
            Legion::FieldID fid_i,
            Legion::FieldID fid_j,
            Legion::FieldID fid_entry
        ) : ctx(ctx), rt(rt),
            kernel_space(kernel_region.get_index_space()),
            kernel_region(kernel_region),
            fid_i(fid_i),
            fid_j(fid_j),
            fid_entry(fid_entry) {}

        virtual Legion::IndexSpace get_kernel_space() const override {
            return kernel_space;
        }

        virtual Legion::LogicalRegion get_kernel_region() const override {
            return kernel_region;
        }

        virtual std::vector<Legion::LogicalRegion>
        get_auxiliary_regions() const override {
            return {};
        }

        virtual Legion::IndexPartition kernel_partition_from_domain_partition(
            Legion::IndexPartition domain_partition
        ) const override {
            return rt->create_partition_by_preimage(ctx,
                domain_partition,
                kernel_region,
                kernel_region,
                fid_j,
                rt->get_index_partition_color_space_name(domain_partition),
                LEGION_COMPUTE_KIND,
                LEGION_AUTO_GENERATE_ID,
                LEGION_SOLVERS_MAPPER_ID
            );
        }

        virtual Legion::IndexPartition kernel_partition_from_range_partition(
            Legion::IndexPartition range_partition
        ) const override {
            return rt->create_partition_by_preimage(ctx,
                range_partition,
                kernel_region,
                kernel_region,
                fid_i,
                rt->get_index_partition_color_space_name(range_partition),
                LEGION_COMPUTE_KIND,
                LEGION_AUTO_GENERATE_ID,
                LEGION_SOLVERS_MAPPER_ID
            );
        }

        virtual Legion::IndexPartition domain_partition_from_kernel_partition(
            Legion::IndexSpace domain_space,
            Legion::IndexPartition kernel_partition
        ) const override {
            return rt->create_partition_by_image(ctx,
                domain_space,
                rt->get_logical_partition(kernel_region, kernel_partition),
                kernel_region,
                fid_j,
                rt->get_index_partition_color_space_name(kernel_partition),
                LEGION_COMPUTE_KIND,
                LEGION_AUTO_GENERATE_ID,
                LEGION_SOLVERS_MAPPER_ID
            );
        }

        virtual Legion::IndexPartition range_partition_from_kernel_partition(
            Legion::IndexSpace range_space,
            Legion::IndexPartition kernel_partition
        ) const override {
            return rt->create_partition_by_image(ctx,
                range_space,
                rt->get_logical_partition(kernel_region, kernel_partition),
                kernel_region,
                fid_i,
                rt->get_index_partition_color_space_name(kernel_partition),
                LEGION_COMPUTE_KIND,
                LEGION_AUTO_GENERATE_ID,
                LEGION_SOLVERS_MAPPER_ID
            );
        }

        virtual void matvec_exclusive(
            DenseDistributedVector<ENTRY_T> &dst_vector,
            const DenseDistributedVector<ENTRY_T> &src_vector,
            Legion::LogicalPartition kernel_partition,
            Legion::IndexPartition ghost_partition
        ) const override {
            const Legion::FieldID fids[3] = {fid_i, fid_j, fid_entry};

            Legion::IndexLauncher launcher{
                LegionSolvers::COOMatvecTask<ENTRY_T, 0, 0, 0>::task_id(
                    kernel_space.get_dim(),
                    src_vector.get_dim(),
                    dst_vector.get_dim()
                ),
                dst_vector.get_color_space(),
                Legion::TaskArgument{&fids, sizeof(Legion::FieldID[3])},
                Legion::ArgumentMap{}
            };
            launcher.map_id = LegionSolvers::LEGION_SOLVERS_MAPPER_ID;

            launcher.add_region_requirement(Legion::RegionRequirement{
                dst_vector.get_logical_partition(),
                0, LEGION_READ_WRITE, LEGION_EXCLUSIVE,
                dst_vector.get_logical_region()
            });
            launcher.add_field(0, dst_vector.get_fid());

            launcher.add_region_requirement(Legion::RegionRequirement{
                kernel_partition,
                0, LEGION_READ_ONLY, LEGION_EXCLUSIVE,
                kernel_region
            });
            launcher.add_field(1, fid_i);
            launcher.add_field(1, fid_j);
            launcher.add_field(1, fid_entry);

            launcher.add_region_requirement(Legion::RegionRequirement{
                rt->get_logical_partition(
                    src_vector.get_logical_region(), ghost_partition
                ),
                0, LEGION_READ_ONLY, LEGION_EXCLUSIVE,
                src_vector.get_logical_region()
            });
            launcher.add_field(2, src_vector.get_fid());

            rt->execute_index_space(ctx, launcher);
        }

    }; // class COOMatrixT


} // namespace LegionSolvers


#endif // LEGION_SOLVERS_COO_MATRIX_HPP
