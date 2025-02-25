#ifndef LEGION_SOLVERS_TASK_BASE_CLASSES_HPP_INCLUDED
#define LEGION_SOLVERS_TASK_BASE_CLASSES_HPP_INCLUDED

#include <string> // for std::string

#include <legion.h> // for Legion::*

#include "LibraryOptions.hpp"           // for LEGION_SOLVERS_USE_*
#include "MetaprogrammingUtilities.hpp" // for TypeList, ListIndex, ...

namespace LegionSolvers {


using LEGION_SOLVERS_SUPPORTED_INDEX_TYPES = TypeList<
#ifdef LEGION_SOLVERS_USE_S32_INDICES
    int,
#endif // LEGION_SOLVERS_USE_S32_INDICES
#ifdef LEGION_SOLVERS_USE_U32_INDICES
    unsigned,
#endif // LEGION_SOLVERS_USE_U32_INDICES
#ifdef LEGION_SOLVERS_USE_S64_INDICES
    long long,
#endif // LEGION_SOLVERS_USE_S64_INDICES
    void>;


using LEGION_SOLVERS_SUPPORTED_ENTRY_TYPES = TypeList<
#ifdef LEGION_SOLVERS_USE_FLOAT
    float,
#endif // LEGION_SOLVERS_USE_FLOAT
#ifdef LEGION_SOLVERS_USE_DOUBLE
    double,
#endif // LEGION_SOLVERS_USE_DOUBLE
    void>;


template <typename T>
constexpr int LEGION_SOLVERS_INDEX_TYPE_INDEX =
    ListIndex<LEGION_SOLVERS_SUPPORTED_INDEX_TYPES, T>::value;
template <typename T>
constexpr int LEGION_SOLVERS_ENTRY_TYPE_INDEX =
    ListIndex<LEGION_SOLVERS_SUPPORTED_ENTRY_TYPES, T>::value;
constexpr int LEGION_SOLVERS_NUM_INDEX_TYPES =
    ListLength<LEGION_SOLVERS_SUPPORTED_INDEX_TYPES>::value;
constexpr int LEGION_SOLVERS_NUM_ENTRY_TYPES =
    ListLength<LEGION_SOLVERS_SUPPORTED_ENTRY_TYPES>::value;
constexpr int LEGION_SOLVERS_NUM_INDEX_TYPES_0 = 1;
constexpr int LEGION_SOLVERS_NUM_INDEX_TYPES_1 =
    LEGION_SOLVERS_NUM_INDEX_TYPES * LEGION_SOLVERS_NUM_INDEX_TYPES_0;
constexpr int LEGION_SOLVERS_NUM_INDEX_TYPES_2 =
    LEGION_SOLVERS_NUM_INDEX_TYPES * LEGION_SOLVERS_NUM_INDEX_TYPES_1;
constexpr int LEGION_SOLVERS_NUM_INDEX_TYPES_3 =
    LEGION_SOLVERS_NUM_INDEX_TYPES * LEGION_SOLVERS_NUM_INDEX_TYPES_2;
constexpr int LEGION_SOLVERS_MAX_DIM_0 = 1;
constexpr int LEGION_SOLVERS_MAX_DIM_1 =
    LEGION_SOLVERS_MAX_DIM * LEGION_SOLVERS_MAX_DIM_0;
constexpr int LEGION_SOLVERS_MAX_DIM_2 =
    LEGION_SOLVERS_MAX_DIM * LEGION_SOLVERS_MAX_DIM_1;
constexpr int LEGION_SOLVERS_MAX_DIM_3 =
    LEGION_SOLVERS_MAX_DIM * LEGION_SOLVERS_MAX_DIM_2;
constexpr int LEGION_SOLVERS_TASK_BLOCK_SIZE = LEGION_SOLVERS_NUM_ENTRY_TYPES *
                                               LEGION_SOLVERS_MAX_DIM_3 *
                                               LEGION_SOLVERS_NUM_INDEX_TYPES_3;


template <
    Legion::TaskID BLOCK_ID,
    template <typename>
    typename TaskClass,
    typename T>
struct TaskT {

    static constexpr Legion::TaskID task_id =
        LEGION_SOLVERS_TASK_ID_ORIGIN +
        LEGION_SOLVERS_TASK_BLOCK_SIZE * BLOCK_ID +
        LEGION_SOLVERS_ENTRY_TYPE_INDEX<T>;

    static std::string task_name() {
        return (
            std::string{TaskClass<T>::task_base_name} + '_' +
            ToString<T>::value()
        );
    }

    static void preregister(bool verbose = true) {
        preregister_task<
            typename TaskClass<T>::return_type,
            TaskClass<T>::task_body>(
            task_id, task_name(), TaskClass<T>::flags, verbose
        );
    }

    // static void announce_cpu(Legion::Context ctx, Legion::Runtime *rt) {
    //     const Legion::Processor proc = rt->get_executing_processor(ctx);
    //     std::cout << "[LegionSolvers] Running CPU task " << task_name()
    //               << " on processor " << proc << '.' << std::endl;
    // }

    // static void preregister_kokkos(bool verbose) {
    //     preregister_kokkos_task<
    //         typename TaskClass<T>::return_type,
    //         TaskClass<T>::template KokkosTaskTemplate
    //     >(task_id, task_name(), TaskClass<T>::flags);
    // }

}; // struct TaskT


template <
    Legion::TaskID BLOCK_ID,
    template <typename, int, typename>
    typename TaskClass,
    typename T,
    int N,
    typename I>
struct TaskTDI {

    static constexpr Legion::TaskID task_id =
        LEGION_SOLVERS_TASK_ID_ORIGIN +
        LEGION_SOLVERS_TASK_BLOCK_SIZE * BLOCK_ID +
        LEGION_SOLVERS_NUM_ENTRY_TYPES * LEGION_SOLVERS_MAX_DIM *
            LEGION_SOLVERS_INDEX_TYPE_INDEX<I> +
        LEGION_SOLVERS_NUM_ENTRY_TYPES * (N - 1) +
        LEGION_SOLVERS_ENTRY_TYPE_INDEX<T>;

    static std::string task_name() {
        return std::string{TaskClass<T, N, I>::task_base_name} + '_' +
               ToString<T>::value() + '_' + std::to_string(N) + '_' +
               ToString<I>::value();
    }

    static void preregister_cpu(bool verbose) {
        preregister_task<
            typename TaskClass<T, N, I>::return_type,
            TaskClass<T, N, I>::task_body>(
            task_id, task_name(), TaskClass<T, N, I>::flags, verbose
        );
    }

    // static void announce_cpu(Legion::Context ctx, Legion::Runtime *rt) {
    //     const Legion::Processor proc = rt->get_executing_processor(ctx);
    //     std::cout << "[LegionSolvers] Running CPU task " << task_name()
    //               << " on processor " << proc << '.' << std::endl;
    // }

    // static void announce_cpu(
    //     Legion::DomainPoint index_point,
    //     Legion::Context ctx,
    //     Legion::Runtime *rt
    // ) {
    //     const Legion::Processor proc = rt->get_executing_processor(ctx);
    //     std::cout << "[LegionSolvers] Running CPU task " << task_name()
    //               << ", index point " << index_point << ", on processor "
    //               << proc << '.' << std::endl;
    // }

    // static void preregister_kokkos(bool verbose) {
    //     preregister_kokkos_task<
    //         typename TaskClass<T, N, I>::return_type,
    //         TaskClass<T, N, I>::template KokkosTaskTemplate>(
    //         task_id,
    //         task_name(),
    //         TaskClass<T, N, I>::is_replicable,
    //         TaskClass<T, N, I>::is_inner,
    //         TaskClass<T, N, I>::is_leaf,
    //         verbose
    //     );
    // }

    // static void announce_kokkos(
    //     Legion::DomainPoint index_point,
    //     const std::type_info &execution_space,
    //     Legion::Context ctx,
    //     Legion::Runtime *rt
    // ) {
    //     const Legion::Processor proc = rt->get_executing_processor(ctx);
    //     std::cout << "[LegionSolvers] Running Kokkos task " << task_name()
    //               << ", index point " << index_point << ", on processor "
    //               << proc << " (kind " << proc.kind() << ", "
    //               << execution_space.name() << ")." << std::endl;
    // }

}; // struct TaskTDI


} // namespace LegionSolvers

#endif // LEGION_SOLVERS_TASK_BASE_CLASSES_HPP_INCLUDED
