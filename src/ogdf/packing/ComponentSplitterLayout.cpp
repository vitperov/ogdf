/** \file
 * \brief Splits and packs the components of a Graph
 *
 * \author Gereon Bartel
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


#include <ogdf/packing/ComponentSplitterLayout.h>
#include <ogdf/packing/TileToRowsCCPacker.h>
#include <ogdf/basic/geometry.h>
#include <ogdf/basic/Math.h>
#include <ogdf/graphalg/ConvexHull.h>
//used for splitting
#include <ogdf/basic/simple_graph_alg.h>
#include <ogdf/basic/GraphCopy.h>


namespace ogdf {

ComponentSplitterLayout::ComponentSplitterLayout()
{
	m_packer.set(new TileToRowsCCPacker);
	m_targetRatio = 1.f;
	m_border = 30;
}


void ComponentSplitterLayout::call(GraphAttributes &GA)
{
	// Only do preparations and call if layout is valid
	if (m_secondaryLayout.valid())
	{
		//first we split the graph into its components
		const Graph& G = GA.constGraph();

		NodeArray<int> componentNumber(G);
		int numberOfComponents = connectedComponents(G, componentNumber);
		if (numberOfComponents == 0) {
			return;
		}

		// intialize the array of lists of nodes contained in a CC
		Array<List<node> > nodesInCC(numberOfComponents);

		for(node v : G.nodes)
			nodesInCC[componentNumber[v]].pushBack(v);

		// Create copies of the connected components and corresponding
		// GraphAttributes
		GraphCopy GC;
		GC.createEmpty(G);

		EdgeArray<edge> auxCopy(G);

		for (int i = 0; i < numberOfComponents; i++)
		{
			GC.initByNodes(nodesInCC[i],auxCopy);
			GraphAttributes cGA(GC, GA.attributes());
			//copy information into copy GA
			for(node v : GC.nodes)
			{
				cGA.width(v) = GA.width(GC.original(v));
				cGA.height(v) = GA.height(GC.original(v));
				cGA.x(v) = GA.x(GC.original(v));
				cGA.y(v) = GA.y(GC.original(v));
			}
			// copy information on edges
			if (GA.has(GraphAttributes::edgeDoubleWeight)) {
				for (edge e : GC.edges) {
					cGA.doubleWeight(e) = GA.doubleWeight(GC.original(e));
				}
			}
			m_secondaryLayout.get().call(cGA);

			//copy layout information back into GA
			for(node v : GC.nodes)
			{
				node w = GC.original(v);
				if (w != nullptr)
				{
					GA.x(w) = cGA.x(v);
					GA.y(w) = cGA.y(v);
					if (GA.has(GraphAttributes::threeD)) {
						GA.z(w) = cGA.z(v);
					}
				}
			}
		}

		// rotate component drawings and call the packer
		reassembleDrawings(GA, nodesInCC);

	}//if valid
}


//-----------------
// geometry helpers

/* copied from multilevelgraph
//moves point set average to origin
void moveToZero()
{
	// move Graph to zero
	double avg_x = 0.0;
	double avg_y = 0.0;
	for(node v : getGraph().nodes) {
		avg_x += x(v);
		avg_y += y(v);
	}
	avg_x /= getGraph().numberOfNodes();
	avg_y /= getGraph().numberOfNodes();
	for(node v : getGraph().nodes) {
		x(v, x(v) - avg_x);
		y(v, y(v) - avg_y);
	}
}
*/

double atan2ex(double y, double x)
{
	double angle = atan2(y, x);

	if (x == 0)
	{
		if (y >= 0) {
			angle = 0.5 * Math::pi;
		} else {
			angle = 1.5 * Math::pi;
		}
	}

	if (y == 0)
	{
		if (x >= 0)
		{
			angle = 0.0;
		} else {
			angle = Math::pi;
		}
	}

	return angle;
}

//TODO: Regard some kind of aspect ration (input)
//(then also the rotation of a single component makes sense)
void ComponentSplitterLayout::reassembleDrawings(GraphAttributes& GA, const Array<List<node> > &nodesInCC)
{
	int numberOfComponents = nodesInCC.size();

	Array<IPoint> box;
	Array<IPoint> offset;
	Array<DPoint> oldOffset;
	Array<double> rotation;
	ConvexHull CH;

	// rotate components and create bounding rectangles

	//iterate through all components and compute convex hull
	for (int j = 0; j < numberOfComponents; j++)
	{
		//todo: should not use std::vector, but in order not
		//to have to change all interfaces, we do it anyway
		std::vector<DPoint> points;

		//collect node positions and at the same time center average
		// at origin
		double avg_x = 0.0;
		double avg_y = 0.0;
		for (node v : nodesInCC[j])
		{
			DPoint dp(GA.x(v), GA.y(v));
			avg_x += dp.m_x;
			avg_y += dp.m_y;
			points.push_back(dp);
		}
		avg_x /= nodesInCC[j].size();
		avg_y /= nodesInCC[j].size();

		//adapt positions to origin
		int count = 0;
		//assume same order of vertices and positions
		for (node v : nodesInCC[j])
		{
			//TODO: I am not sure if we need to update both
			GA.x(v) = GA.x(v) - avg_x;
			GA.y(v) = GA.y(v) - avg_y;
			points.at(count).m_x -= avg_x;
			points.at(count).m_y -= avg_y;

			count++;
		}

		// calculate convex hull
		DPolygon hull = CH.call(points);

		double best_area = numeric_limits<double>::max();
		DPoint best_normal;
		double best_width = 0.0;
		double best_height = 0.0;

		// find best rotation by using every face as rectangle border once.
		for (DPolygon::iterator j = hull.begin(); j != hull.end(); ++j) {
			DPolygon::iterator k = hull.cyclicSucc(j);

			double dist = 0.0;
			DPoint norm = CH.calcNormal(*k, *j);
			for (const DPoint &z : hull) {
				double d = CH.leftOfLine(norm, z, *k);
				if (d > dist) {
					dist = d;
				}
			}

			double left = 0.0;
			double right = 0.0;
			norm = CH.calcNormal(DPoint(0, 0), norm);
			for (const DPoint &z : hull) {
				double d = CH.leftOfLine(norm, z, *k);
				if (d > left) {
					left = d;
				}
				else if (d < right) {
					right = d;
				}
			}
			double width = left - right;

			dist = max(dist, 1.0);
			width = max(width, 1.0);

			double area = dist * width;

			if (area <= best_area) {
				best_height = dist;
				best_width = width;
				best_area = area;
				best_normal = CH.calcNormal(*k, *j);
			}
		}

		if (hull.size() <= 1) {
			best_height = 1.0;
			best_width = 1.0;
			best_area = 1.0;
			best_normal = DPoint(1.0, 1.0);
		}

		double angle = -atan2(best_normal.m_y, best_normal.m_x) + 1.5 * Math::pi;
		if (best_width < best_height) {
			angle += 0.5f * Math::pi;
			double temp = best_height;
			best_height = best_width;
			best_width = temp;
		}
		rotation.grow(1, angle);
		double left = hull.front().m_x;
		double top = hull.front().m_y;
		double bottom = hull.front().m_y;
		// apply rotation to hull and calc offset
		for (DPoint tempP : hull) {
			double ang = atan2(tempP.m_y, tempP.m_x);
			double len = sqrt(tempP.m_x*tempP.m_x + tempP.m_y*tempP.m_y);
			ang += angle;
			tempP.m_x = cos(ang) * len;
			tempP.m_y = sin(ang) * len;

			if (tempP.m_x < left) {
				left = tempP.m_x;
			}
			if (tempP.m_y < top) {
				top = tempP.m_y;
			}
			if (tempP.m_y > bottom) {
				bottom = tempP.m_y;
			}
		}
		oldOffset.grow(1, DPoint(left + 0.5 * static_cast<double>(m_border), -1.0 * best_height + 1.0 * bottom + 0.0 * top + 0.5 * (double)m_border));

		// save rect
		int w = static_cast<int>(best_width);
		int h = static_cast<int>(best_height);
		box.grow(1, IPoint(w + m_border, h + m_border));
	}// components

	offset.init(box.size());

	// call packer
	m_packer.get().call(box, offset, m_targetRatio);

	int index = 0;
	// Apply offset and rebuild Graph
	for (int j = 0; j < numberOfComponents; j++)
	{
		double angle = rotation[index];
		// apply rotation and offset to all nodes

		for (node v : nodesInCC[j])
		{
			double x = GA.x(v);
			double y = GA.y(v);
			double ang = atan2(y, x);
			double len = sqrt(x*x + y*y);
			ang += angle;
			x = cos(ang) * len;
			y = sin(ang) * len;

			x += static_cast<double>(offset[index].m_x);
			y += static_cast<double>(offset[index].m_y);

			x -= oldOffset[index].m_x;
			y -= oldOffset[index].m_y;

			GA.x(v) = x;
			GA.y(v) = y;

		}// while nodes in component

		index++;
	} // for components

	//now we center the whole graph again
	//TODO: why?
	//const Graph& G = GA.constGraph();
	//for(node v : G.nodes)
	//MLG.moveToZero();
}


} // namespace ogdf
