/***************************************************************************
 *  graph.cpp - Graphviz graph wrapper
 *
 *  Created:   Wed 14 Apr 22:52:49 CEST 2021
 *  Copyright  2021  Till Hofmann <hofmann@kbsg.rwth-aachen.de>
 ****************************************************************************/
/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.md file.
 */

#include "utilities/graphviz/graphviz.h"

#include <cgraph.h>
#include <cstddef>
#include <gvc.h>
#include <optional>
#include <stdexcept>

namespace utilities::graphviz {

Graph::Graph() : Graph("G", GraphType::Directed)
{
}

Graph::Graph(const std::string &name, GraphType type) : graph_name(name)
{
	Agdesc_t ag_type;
	switch (type) {
	case GraphType::Undirected: ag_type = Agundirected; break;
	case GraphType::StrictUndirected: ag_type = Agstrictundirected; break;
	case GraphType::Directed: ag_type = Agdirected; break;
	case GraphType::StrictDirected: ag_type = Agstrictdirected; break;
	}
	context = gvContext();
	graph   = agopen(graph_name.data(), ag_type, nullptr);
}

Graph::~Graph()
{
	if (layout_created) {
		gvFreeLayout(context, graph);
	}
	agclose(graph);
	gvFreeContext(context);
}

void
Graph::set_default_node_property(const std::string &property, const std::string &value)
{
	agattr(graph, AGNODE, std::string(property).data(), std::string(value).data());
}

Node
Graph::add_node(std::string label, std::optional<std::string> identifier)
{
	std::string id;
	if (identifier) {
		id = *identifier;
	} else {
		auto next_id = ++last_node_id;
		id           = std::to_string(next_id);
	}
	auto ag_node = agnode(graph, id.data(), 1);
	Node node{ag_node};
	node.set_property("label", label);
	return node;
}

void
Graph::add_edge(const Node &source, const Node &target, std::string label)
{
	auto edge = agedge(
	  graph, source.get_agnode(), target.get_agnode(), std::to_string(++last_edge_id).data(), 1);
	agsafeset(edge, std::string("label").data(), label.data(), std::string().data());
}

void
Graph::set_property(const std::string &property, const std::string &value)
{
	agsafeset(graph, std::string(property).data(), std::string(value).data(), std::string().data());
}

bool
Graph::has_node(std::string identifier)
{
	return agnode(graph, identifier.data(), 0) != nullptr;
}

std::string
Graph::to_dot()
{
	if (!layout_created) {
		layout();
	}
	char *       res;
	unsigned int res_length;
	gvRenderData(context, graph, "dot", &res, &res_length);
	const std::string dot{res};
	gvFreeRenderData(res);
	return dot;
}

void
Graph::render_to_file(const std::filesystem::path &output_path)
{
	if (!layout_created) {
		layout();
	}
	const std::string extension = output_path.extension().string();
	if (extension.empty()) {
		throw std::invalid_argument(std::string("Missing file extension in filename ")
		                            + output_path.string());
	}
	const std::string filetype(std::next(std::begin(extension)), std::end(extension));
	gvRenderFilename(context, graph, filetype.c_str(), output_path.c_str());
}

void
Graph::layout()
{
	if (layout_created) {
		gvFreeLayout(context, graph);
	}
	gvLayout(context, graph, "dot");
	layout_created = true;
}

} // namespace utilities::graphviz
