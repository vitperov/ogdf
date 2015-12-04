/** \file
 * \brief Declaration of class ShortestPathWithBFM which computes
 *        shortest paths via Bellman-Ford-Moore.
 *
 * \author Gunnar W. Klau
 *
 * \par License:
 * This file is part of the Open Graph Drawing Framework (OGDF).
 *
 * \par
 * Copyright (C)<br>
 * See README.txt in the root directory of the OGDF installation for details.
 *
 * \par
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2 or 3 as published by the Free Software Foundation;
 * see the file LICENSE.txt included in the packaging of this file
 * for details.
 *
 * \par
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * \see  http://www.gnu.org/copyleft/gpl.html
 ***************************************************************/

#pragma once

#include <ogdf/module/ShortestPathModule.h>


namespace ogdf {

//! Computes single-source shortest-paths with Bellman-Ford-Moore's algorithm.
/**
 * @ingroup ga-sp
 */
class OGDF_EXPORT ShortestPathWithBFM : public ShortestPathModule
{
public:
	ShortestPathWithBFM() { }

	// computes shortest paths
	// Precond.:
	//
	// returns false iff the graph contains a negative cycle
	virtual bool call(
		const Graph &G,                   // directed graph
		const node s,					  // source node
		const EdgeArray<int> &length,     // length of an edge
		NodeArray<int> &d,				  // contains shortest path distances after call
		NodeArray<edge> &pi
	) override;

};


} // end namespace ogdf
