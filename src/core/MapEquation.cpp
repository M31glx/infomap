/*
 * MapEquation.h
 *
 *  Created on: 25 feb 2015
 *      Author: Daniel
 */
#include "MapEquation.h"
#include "../utils/infomath.h"
#include "../io/convert.h"
#include "../io/Config.h"
#include "../utils/Log.h"
#include "../utils/VectorMap.h"
#include "NodeBase.h"
#include "Node.h"
#include "FlowData.h"
#include <vector>
#include <map>
#include <iostream>

namespace infomap {

	using NodeType = Node<FlowData>;

// ===================================================
// Getters
// ===================================================


// ===================================================
// IO
// ===================================================

std::ostream& MapEquation::print(std::ostream& out) const {
	return out << indexCodelength << " + " << moduleCodelength << " = " <<	io::toPrecision(codelength);
}

// std::ostream& operator<<(std::ostream& out, const MapEquation& mapEq) {
// 	return mapEq.print(out);
// }


// ===================================================
// Init
// ===================================================

void MapEquation::init(const Config& config)
{
	Log(3) << "MapEquation::init()...\n";
}

void MapEquation::initNetwork(NodeBase& root)
{
	Log(3) << "MapEquation::initNetwork()...\n";

	nodeFlow_log_nodeFlow = 0.0;
	for (NodeBase& node : root)
	{
		nodeFlow_log_nodeFlow += infomath::plogp(getNode(node).data.flow);
	}
	initSubNetwork(root);
}

void MapEquation::initSuperNetwork(NodeBase& root)
{
	Log(3) << "MapEquation::initSuperNetwork()...\n";

	nodeFlow_log_nodeFlow = 0.0;
	for (NodeBase& node : root)
	{
		nodeFlow_log_nodeFlow += infomath::plogp(getNode(node).data.enterFlow);
	}
}

void MapEquation::initSubNetwork(NodeBase& root)
{
	exitNetworkFlow = getNode(root).data.exitFlow;
	exitNetworkFlow_log_exitNetworkFlow = infomath::plogp(exitNetworkFlow);
	// Debug() << "\ninitSubNetwork() exitNetworkFlow: " << exitNetworkFlow << ", exitNetworkFlow_log_exitNetworkFlow: " << exitNetworkFlow_log_exitNetworkFlow << "\n";
}

void MapEquation::initPartition(std::vector<NodeBase*>& nodes)
{
	calculateCodelength(nodes);
}


// ===================================================
// Codelength
// ===================================================

void MapEquation::calculateCodelength(std::vector<NodeBase*>& nodes)
{
	calculateCodelengthTerms(nodes);

	calculateCodelengthFromCodelengthTerms();
}

void MapEquation::calculateCodelengthTerms(std::vector<NodeBase*>& nodes)
{
	enter_log_enter = 0.0;
	flow_log_flow = 0.0;
	exit_log_exit = 0.0;
	enterFlow = 0.0;

	// For each module
	for (NodeBase* n : nodes)
	{
		const NodeType& node = getNode(*n);
		// own node/module codebook
		flow_log_flow += infomath::plogp(node.data.flow + node.data.exitFlow);

		// use of index codebook
		enter_log_enter += infomath::plogp(node.data.enterFlow);
		exit_log_exit += infomath::plogp(node.data.exitFlow);
		enterFlow += node.data.enterFlow;
	}
	enterFlow += exitNetworkFlow;
	enterFlow_log_enterFlow = infomath::plogp(enterFlow);
	// Debug() << "\nexitNetworkFlow: " << exitNetworkFlow << ", exit_log_exit: " << exit_log_exit << ", enterFlow_log_enterFlow: " << enterFlow_log_enterFlow << ", flow_log_flow: " << flow_log_flow << "\n";
}

void MapEquation::calculateCodelengthFromCodelengthTerms()
{
	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
	// Debug() << "\n=> indexCodelength = " << enterFlow_log_enterFlow << " - " << enter_log_enter << " - " << exitNetworkFlow_log_exitNetworkFlow << "\n";
}

double MapEquation::calcCodelength(const NodeBase& parent) const
{
	return parent.isLeafModule()? calcCodelengthOnModuleOfLeafNodes(parent) : calcCodelengthOnModuleOfModules(parent);
}

double MapEquation::calcCodelengthOnModuleOfLeafNodes(const NodeBase& p) const
{
	auto& parent = getNode(p);
	double parentFlow = parent.data.flow;
	double parentExit = parent.data.exitFlow;
	double totalParentFlow = parentFlow + parentExit;
	if (totalParentFlow < 1e-16)
		return 0.0;

	double indexLength = 0.0;
	double sumFlow = 0.0;
	for (const auto& node : parent)
	{
		sumFlow += getNode(node).data.flow;
		indexLength -= infomath::plogp(getNode(node).data.flow / totalParentFlow);
	}
	indexLength -= infomath::plogp(parentExit / totalParentFlow);

	indexLength *= totalParentFlow;

	// Debug() << "calcCodelengthOnModuleOfLeafNodes(): " << indexLength/totalParentFlow << " * " << totalParentFlow << " = " << indexLength << "\n";
	return indexLength;
}

double MapEquation::calcCodelengthOnModuleOfModules(const NodeBase& p) const
{
	auto& parent = getNode(p);
	double parentFlow = parent.data.flow;
	double parentExit = parent.data.exitFlow;
	if (parentFlow < 1e-16)
		return 0.0;

	// H(x) = -xlog(x), T = q + SUM(p), q = exitFlow, p = enterFlow
	// Normal format
	// L = q * -log(q/T) + p * SUM(-log(p/T))
	// Compact format
	// L = T * ( H(q/T) + SUM( H(p/T) ) )
	// Expanded format
	// L = q * -log(q) - q * -log(T) + SUM( p * -log(p) - p * -log(T) )
	//   = T * log(T) - q*log(q) - SUM( p*log(p) )
	// As T is not known, use expanded format to avoid two loops
	double sumEnter = 0.0;
	double sumEnterLogEnter = 0.0;
	for (const auto& n : parent)
	{
		auto& node = getNode(n);
		sumEnter += node.data.enterFlow; // rate of enter to finer level
		sumEnterLogEnter += infomath::plogp(node.data.enterFlow);
	}
	// The possibilities from this module: Either exit to coarser level or enter one of its children
	double totalCodewordUse = parentExit + sumEnter;
	double L = infomath::plogp(totalCodewordUse) - sumEnterLogEnter - infomath::plogp(parentExit);

	// Debug() << "\ncalcCodelengthOnModuleOfModules(): " << infomath::plogp(totalCodewordUse) << " - " << sumEnterLogEnter << " - " << infomath::plogp(parentExit) << " = " << L;
	// return infomath::plogp(totalCodewordUse) - sumEnterLogEnter - infomath::plogp(parentExit);
	return L;
}


double MapEquation::getDeltaCodelengthOnMovingNode(NodeBase& curr,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	using infomath::plogp;
	auto& current = getNode(curr);
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	double delta_enter = plogp(enterFlow + deltaEnterExitOldModule - deltaEnterExitNewModule) - enterFlow_log_enterFlow;

	double delta_enter_log_enter = \
			- plogp(moduleFlowData[oldModule].enterFlow) \
			- plogp(moduleFlowData[newModule].enterFlow) \
			+ plogp(moduleFlowData[oldModule].enterFlow - current.data.enterFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].enterFlow + current.data.enterFlow - deltaEnterExitNewModule);

	double delta_exit_log_exit = \
			- plogp(moduleFlowData[oldModule].exitFlow) \
			- plogp(moduleFlowData[newModule].exitFlow) \
			+ plogp(moduleFlowData[oldModule].exitFlow - current.data.exitFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + current.data.exitFlow - deltaEnterExitNewModule);

	double delta_flow_log_flow = \
			- plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) \
			- plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow) \
			+ plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow \
					- current.data.exitFlow - current.data.flow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow \
					+ current.data.exitFlow + current.data.flow - deltaEnterExitNewModule);

	double deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
	// Debug() << "\ndeltaL = " << delta_enter << " - " << delta_enter_log_enter << " - " <<
	// delta_exit_log_exit << " + " << delta_flow_log_flow << " = " << deltaL;
	return deltaL;
}

void MapEquation::updateCodelengthOnMovingNode(NodeBase& curr,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	using infomath::plogp;
	auto& current = getNode(curr);
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	enterFlow -= \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	enter_log_enter -= \
			plogp(moduleFlowData[oldModule].enterFlow) + \
			plogp(moduleFlowData[newModule].enterFlow);
	exit_log_exit -= \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	flow_log_flow -= \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);


	moduleFlowData[oldModule] -= current.data;
	moduleFlowData[newModule] += current.data;

	moduleFlowData[oldModule].enterFlow += deltaEnterExitOldModule;
	moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
	moduleFlowData[newModule].enterFlow -= deltaEnterExitNewModule;
	moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;

	enterFlow += \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	enter_log_enter += \
			plogp(moduleFlowData[oldModule].enterFlow) + \
			plogp(moduleFlowData[newModule].enterFlow);
	exit_log_exit += \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	flow_log_flow += \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);

	enterFlow_log_enterFlow = plogp(enterFlow);

	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
}

NodeBase* MapEquation::createNode() const
{
	return new NodeType();
}

NodeBase* MapEquation::createNode(const NodeBase& other) const
{
	return new NodeType(static_cast<const NodeType&>(other));
}

NodeBase* MapEquation::createNode(FlowDataType flowData) const
{
	return new NodeType(static_cast<const NodeType&>(flowData));
}

const NodeType& MapEquation::getNode(const NodeBase& other) const
{
	return static_cast<const NodeType&>(other);
}

// ===================================================
// Debug
// ===================================================

void MapEquation::printDebug()
{
	Debug() << "(enterFlow_log_enterFlow: " << enterFlow_log_enterFlow << ", " <<
			"enter_log_enter: " << enter_log_enter << ", " <<
			"exitNetworkFlow_log_exitNetworkFlow: " << exitNetworkFlow_log_exitNetworkFlow << ") ";
//	Debug() << "enterFlow_log_enterFlow: " << enterFlow_log_enterFlow << "\n" <<
//			"enter_log_enter: " << enter_log_enter << "\n" <<
//			"exitNetworkFlow_log_exitNetworkFlow: " << exitNetworkFlow_log_exitNetworkFlow << "\n";
//	Debug() << "exit_log_exit: " << exit_log_exit << "\n" <<
//			"flow_log_flow: " << flow_log_flow << "\n" <<
//			"nodeFlow_log_nodeFlow: " << nodeFlow_log_nodeFlow << "\n";
}

}
