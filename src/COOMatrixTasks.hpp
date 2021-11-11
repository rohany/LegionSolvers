#ifndef LEGION_SOLVERS_COO_MATRIX_TASKS_HPP
#define LEGION_SOLVERS_COO_MATRIX_TASKS_HPP

#include <Kokkos_Core.hpp>
#include <legion.h>

#include "KokkosUtilities.hpp"
#include "TaskBaseClasses.hpp"
#include "TaskIDs.hpp"


namespace LegionSolvers {


    template <typename ExecutionSpace, typename ENTRY_T,
              int KERNEL_DIM, int DOMAIN_DIM, int RANGE_DIM>
    struct KokkosCOOMatvecFunctor {

        const Legion::Rect<DOMAIN_DIM> domain_rect;

        const Legion::Rect<RANGE_DIM> range_rect;

        const Realm::AffineAccessor<
            Legion::Point<RANGE_DIM>, KERNEL_DIM, Legion::coord_t
        > i_reader;

        const Realm::AffineAccessor<
            Legion::Point<DOMAIN_DIM>, KERNEL_DIM, Legion::coord_t
        > j_reader;

        const Realm::AffineAccessor<ENTRY_T, KERNEL_DIM, Legion::coord_t>
        entry_reader;

        const Realm::AffineAccessor<ENTRY_T, DOMAIN_DIM, Legion::coord_t>
        input_reader;

        const Legion::ReductionAccessor<
            Legion::SumReduction<ENTRY_T>, false,
            RANGE_DIM, Legion::coord_t,
            Realm::AffineAccessor<ENTRY_T, RANGE_DIM, Legion::coord_t>
        > output_writer;

        KOKKOS_INLINE_FUNCTION void operator()(int a) const {
            const Legion::Point<1> index{a};
            const Legion::Point<RANGE_DIM> i = i_reader[index];
            const Legion::Point<DOMAIN_DIM> j = j_reader[index];
            const ENTRY_T entry = entry_reader[index];
            if (range_rect.contains(i) && domain_rect.contains(j)) {
                output_writer[i] <<= entry * input_reader[j];
            }
        }

        KOKKOS_INLINE_FUNCTION void operator()(int a, int b) const {
            const Legion::Point<2> index{a, b};
            const Legion::Point<RANGE_DIM> i = i_reader[index];
            const Legion::Point<DOMAIN_DIM> j = j_reader[index];
            const ENTRY_T entry = entry_reader[index];
            if (range_rect.contains(i) && domain_rect.contains(j)) {
                output_writer[i] <<= entry * input_reader[j];
            }
        }

        KOKKOS_INLINE_FUNCTION void operator()(int a, int b, int c) const {
            const Legion::Point<3> index{a, b, c};
            const Legion::Point<RANGE_DIM> i = i_reader[index];
            const Legion::Point<DOMAIN_DIM> j = j_reader[index];
            const ENTRY_T entry = entry_reader[index];
            if (range_rect.contains(i) && domain_rect.contains(j)) {
                output_writer[i] <<= entry * input_reader[j];
            }
        }

        KOKKOS_INLINE_FUNCTION void operator()(
            int a, int b, int c, int d
        ) const {
            const Legion::Point<4> index{a, b, c, d};
            const Legion::Point<RANGE_DIM> i = i_reader[index];
            const Legion::Point<DOMAIN_DIM> j = j_reader[index];
            const ENTRY_T entry = entry_reader[index];
            if (range_rect.contains(i) && domain_rect.contains(j)) {
                output_writer[i] <<= entry * input_reader[j];
            }
        }

        KOKKOS_INLINE_FUNCTION void operator()(
            int a, int b, int c, int d, int e
        ) const {
            const Legion::Point<5> index{a, b, c, d, e};
            const Legion::Point<RANGE_DIM> i = i_reader[index];
            const Legion::Point<DOMAIN_DIM> j = j_reader[index];
            const ENTRY_T entry = entry_reader[index];
            if (range_rect.contains(i) && domain_rect.contains(j)) {
                output_writer[i] <<= entry * input_reader[j];
            }
        }

        KOKKOS_INLINE_FUNCTION void operator()(
            int a, int b, int c, int d, int e, int f
        ) const {
            const Legion::Point<6> index{a, b, c, d, e, f};
            const Legion::Point<RANGE_DIM> i = i_reader[index];
            const Legion::Point<DOMAIN_DIM> j = j_reader[index];
            const ENTRY_T entry = entry_reader[index];
            if (range_rect.contains(i) && domain_rect.contains(j)) {
                output_writer[i] <<= entry * input_reader[j];
            }
        }

        KOKKOS_INLINE_FUNCTION void operator()(
            int a, int b, int c, int d, int e, int f, int g
        ) const {
            const Legion::Point<7> index{a, b, c, d, e, f, g};
            const Legion::Point<RANGE_DIM> i = i_reader[index];
            const Legion::Point<DOMAIN_DIM> j = j_reader[index];
            const ENTRY_T entry = entry_reader[index];
            if (range_rect.contains(i) && domain_rect.contains(j)) {
                output_writer[i] <<= entry * input_reader[j];
            }
        }

        KOKKOS_INLINE_FUNCTION void operator()(
            int a, int b, int c, int d,
            int e, int f, int g, int h
        ) const {
            const Legion::Point<8> index{a, b, c, d, e, f, g, h};
            const Legion::Point<RANGE_DIM> i = i_reader[index];
            const Legion::Point<DOMAIN_DIM> j = j_reader[index];
            const ENTRY_T entry = entry_reader[index];
            if (range_rect.contains(i) && domain_rect.contains(j)) {
                output_writer[i] <<= entry * input_reader[j];
            }
        }

        KOKKOS_INLINE_FUNCTION void operator()(
            int a, int b, int c, int d, int e,
            int f, int g, int h, int k
        ) const {
            const Legion::Point<9> index{a, b, c, d, e, f, g, h, k};
            const Legion::Point<RANGE_DIM> i = i_reader[index];
            const Legion::Point<DOMAIN_DIM> j = j_reader[index];
            const ENTRY_T entry = entry_reader[index];
            if (range_rect.contains(i) && domain_rect.contains(j)) {
                output_writer[i] <<= entry * input_reader[j];
            }
        }

    }; // struct KokkosCOOMatvecFunctor


    template <typename ENTRY_T, int KERNEL_DIM, int DOMAIN_DIM, int RANGE_DIM>
    struct COOMatvecTask : TaskTDDD<COO_MATVEC_TASK_BLOCK_ID,
                                    COOMatvecTask, ENTRY_T,
                                    KERNEL_DIM, DOMAIN_DIM, RANGE_DIM> {

        static constexpr const char *task_base_name = "coo_matvec";

        static constexpr bool is_leaf = true;

        using return_type = void;

        template <typename ExecutionSpace>
        struct KokkosTaskTemplate {

            static void task_body(
                const Legion::Task *task,
                const std::vector<Legion::PhysicalRegion> &regions,
                Legion::Context ctx, Legion::Runtime *rt
            );

        }; // struct KokkosTaskBody

    }; // struct COOMatvecTask


} // namespace LegionSolvers


#endif // LEGION_SOLVERS_COO_MATRIX_TASKS_HPP