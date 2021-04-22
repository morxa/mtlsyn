/***************************************************************************
 *  app.cpp - The tool's main app library
 *
 *  Created:   Mon 19 Apr 13:43:55 CEST 2021
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
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include "app/app.h"

#include "automata/ta.h"
#include "automata/ta.pb.h"
#include "automata/ta_product.h"
#include "automata/ta_proto.h"
#include "automata/ta_regions.h"
#include "mtl/MTLFormula.h"
#include "mtl/mtl.pb.h"
#include "mtl/mtl_proto.h"
#include "mtl_ata_translation/translator.h"
#include "synchronous_product/create_controller.h"
#include "synchronous_product/heuristics.h"
#include "synchronous_product/search.h"
#include "synchronous_product/search_tree.h"
#include "visualization/ta_to_graphviz.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>

#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace app {

namespace {
std::unique_ptr<synchronous_product::Heuristic<long, std::vector<std::string>, std::string>>
create_heuristic(const std::string &name)
{
	if (name == "time") {
		return std::make_unique<
		  synchronous_product::TimeHeuristic<long, std::vector<std::string>, std::string>>();
	} else if (name == "bfs") {
		return std::make_unique<
		  synchronous_product::BfsHeuristic<long, std::vector<std::string>, std::string>>();
	} else if (name == "dfs") {
		return std::make_unique<
		  synchronous_product::DfsHeuristic<long, std::vector<std::string>, std::string>>();
	}
	throw std::invalid_argument("Unknown heuristic: " + name);
}

} // namespace

Launcher::Launcher(int argc, const char *const argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	spdlog::set_pattern("%v");
	spdlog::set_level(spdlog::level::trace);
	parse_command_line(argc, argv);
}

void
Launcher::parse_command_line(int argc, const char *const argv[])
{
	using boost::program_options::bool_switch;
	using boost::program_options::value;
	boost::program_options::options_description options("Allowed options");

	// clang-format off
	options.add_options()
    ("help,h", "Print help message")
    ("plant,p", value(&plant_path)->required(), "The path to the plant proto")
    ("specification,s", value(&specification_path)->required(), "The path to the specification proto")
    ("controller-action,c", value<std::vector<std::string>>(), "The actions controlled by the controller")
    ("single-threaded", bool_switch()->default_value(false), "run single-threaded")
    ("visualize-plant", value(&plant_graph_path), "Generate a dot graph of the input plant")
    ("output,o", value(&controller_path), "Output path to write the controller to")
    ("heuristic", value(&heuristic)->default_value("time"), "The heuristic to use (one of 'time', 'bfs', 'dfs')")
    ;
	// clang-format on

	boost::program_options::variables_map variables;
	boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options),
	                              variables);
	if (variables.count("help")) {
		SPDLOG_INFO(options);
		show_help = true;
		return;
	}
	boost::program_options::notify(variables);
	multi_threaded = !variables["single-threaded"].as<bool>();
	// Convert the vector of actions into a set of actions.
	if (variables.count("controller-action")) {
		std::copy(std::begin(variables["controller-action"].as<std::vector<std::string>>()),
		          std::end(variables["controller-action"].as<std::vector<std::string>>()),
		          std::inserter(controller_actions, std::end(controller_actions)));
	}
}

void
Launcher::read_proto_from_file(const std::filesystem::path &path, google::protobuf::Message *output)
{
	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		throw std::invalid_argument(
		  fmt::format("Could not open plant file '{}' (errno: {})", path.c_str(), errno));
	}
	google::protobuf::io::FileInputStream stream(fd);
	stream.SetCloseOnDelete(true);
	if (!google::protobuf::TextFormat::Parse(&stream, output)) {
		throw std::invalid_argument(fmt::format("Failed to read proto from file '{}", path.c_str()));
	}
}

void
Launcher::run()
{
	if (show_help) {
		return;
	}
	automata::ta::proto::ProductAutomaton ta_proto;
	SPDLOG_INFO("Reading plant TA from '{}'", plant_path.c_str());
	read_proto_from_file(plant_path, &ta_proto);
	auto plant = automata::ta::parse_product_proto(ta_proto);
	SPDLOG_DEBUG("TA:\n{}", plant);
	if (!plant_graph_path.empty()) {
		visualization::ta_to_graphviz(plant).render_to_file(plant_graph_path);
	}
	SPDLOG_INFO("Reading MTL specification of undesired behaviors from '{}'",
	            specification_path.c_str());
	logic::proto::MTLFormula spec_proto;
	read_proto_from_file(specification_path, &spec_proto);
	auto                                            spec = logic::parse_proto(spec_proto);
	std::set<logic::AtomicProposition<std::string>> aps;
	std::transform(std::begin(plant.get_alphabet()),
	               std::end(plant.get_alphabet()),
	               std::inserter(aps, std::end(aps)),
	               [](const auto &symbol) { return logic::AtomicProposition<std::string>{symbol}; });
	auto ata = mtl_ata_translation::translate(spec, aps);
	SPDLOG_DEBUG("Specification: {}", spec);
	SPDLOG_DEBUG("ATA:\n{}", ata);
	std::set<std::string> environment_actions;
	std::set_difference(std::begin(plant.get_alphabet()),
	                    std::end(plant.get_alphabet()),
	                    std::begin(controller_actions),
	                    std::end(controller_actions),
	                    std::inserter(environment_actions, std::end(environment_actions)));
	SPDLOG_INFO("Controller actions: {}", fmt::join(controller_actions, ", "));
	SPDLOG_INFO("Environment actions: {}", fmt::join(environment_actions, ", "));
	SPDLOG_INFO("Initializing search");
	const auto K = std::max(plant.get_largest_constant(), spec.get_largest_constant());
	synchronous_product::TreeSearch search(&plant,
	                                       &ata,
	                                       controller_actions,
	                                       environment_actions,
	                                       K,
	                                       true,
	                                       true,
	                                       create_heuristic(heuristic));
	SPDLOG_INFO("Running search {}", multi_threaded ? "multi-threaded" : "single-threaded");
	search.build_tree(multi_threaded);
	SPDLOG_INFO("Search complete!");
	SPDLOG_DEBUG("Search tree:\n{}", synchronous_product::node_to_string(*search.get_root(), true));
	SPDLOG_INFO("Creating controller");
	auto controller = controller_synthesis::create_controller(search.get_root(), K);
	if (!controller_path.empty()) {
		SPDLOG_INFO("Writing controller to '{}'", controller_path.c_str());
		visualization::ta_to_graphviz(controller).render_to_file(controller_path);
	}
}

} // namespace app
