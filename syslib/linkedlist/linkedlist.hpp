/**
 ******************************************************************************
 * @file    linkedlist.hpp
 * @author  Samuel Almeida Rocha 
 * @version 1.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_SYSLIB_LINKEDLIST
 * @brief   Intrusive singly-linked list.
 *

 */

#ifndef HELIOS_SYSLIB_LINKEDLIST_HPP_
#define HELIOS_SYSLIB_LINKEDLIST_HPP_

#include <cstddef>

namespace hel
{

/**
 * @brief   Intrusive singly-linked list.
 * @details
 *   - No dynamic allocation; nodes are embedded in the element type.
 *   - No exceptions; operations are bounds-safe.
 * @ingroup HELIOS_SYSLIB_LINKEDLIST
 */
class LinkedList
{
public:
};

} // namespace hel

#endif // HELIOS_SYSLIB_LINKEDLIST_HPP_
