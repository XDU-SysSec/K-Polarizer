//===- PTACallGraph.cpp -- Call graph used internally in SVF------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//


/*
 * PTACallGraph.cpp
 *
 *  Created on: Nov 7, 2013
 *      Author: Yulei Sui
 */

#include "Graphs/PTACallGraph.h"
#include "Graphs/CallGraph.h"
#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"
#include <sstream>

//author added
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <chrono>
#include "SVF-LLVM/LLVMModule.h"

using namespace SVF;
using namespace SVFUtil;

PTACallGraph::CallSiteToIdMap PTACallGraph::csToIdMap;
PTACallGraph::IdToCallSiteMap PTACallGraph::idToCSMap;
CallSiteID PTACallGraph::totalCallSiteNum=1;

/// Add direct and indirect callsite
//@{
void PTACallGraphEdge::addDirectCallSite(const CallICFGNode* call)
{
    assert(call->getCalledFunction() && "not a direct callsite??");
    directCalls.insert(call);
}

void PTACallGraphEdge::addInDirectCallSite(const CallICFGNode* call)
{
    assert((nullptr == call->getCalledFunction() || !SVFUtil::isa<FunValVar>(SVFUtil::getForkedFun(call))) &&
           "not an indirect callsite??");
    indirectCalls.insert(call);
}
//@}

const std::string PTACallGraphEdge::toString() const
{
    std::string str;
    std::stringstream  rawstr(str);
    rawstr << "CallSite ID: " << getCallSiteID();
    if(isDirectCallEdge())
        rawstr << "direct call";
    else
        rawstr << "indirect call";
    rawstr << "[" << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string PTACallGraphNode::toString() const
{
    std::string str;
    std::stringstream  rawstr(str);
    rawstr << "PTACallGraphNode ID: " << getId() << " {fun: " << fun->getName() << "}";
    return rawstr.str();
}

bool PTACallGraphNode::isReachableFromProgEntry() const
{
    std::stack<const PTACallGraphNode*> nodeStack;
    NodeBS visitedNodes;
    nodeStack.push(this);
    visitedNodes.set(getId());

    while (nodeStack.empty() == false)
    {
        PTACallGraphNode* node = const_cast<PTACallGraphNode*>(nodeStack.top());
        nodeStack.pop();

        if (SVFUtil::isProgEntryFunction(node->getFunction()))
            return true;

        for (const_iterator it = node->InEdgeBegin(), eit = node->InEdgeEnd(); it != eit; ++it)
        {
            PTACallGraphEdge* edge = *it;
            if (visitedNodes.test_and_set(edge->getSrcID()))
                nodeStack.push(edge->getSrcNode());
        }
    }

    return false;
}


/// Constructor
PTACallGraph::PTACallGraph(CGEK k): kind(k)
{
    callGraphNodeNum = 0;
    numOfResolvedIndCallEdge = 0;
}

/// Copy constructor
PTACallGraph::PTACallGraph(const CallGraph& other)
{
    callGraphNodeNum = other.getTotalNodeNum();
    numOfResolvedIndCallEdge = 0;
    kind = NormCallGraph;

    /// copy call graph nodes
    for (const auto& item : other)
    {
        const CallGraphNode* cgn = item.second;
        PTACallGraphNode* callGraphNode = new PTACallGraphNode(cgn->getId(), cgn->getFunction());
        addGNode(cgn->getId(),callGraphNode);
        funToCallGraphNodeMap[cgn->getFunction()] = callGraphNode;
    }

    /// copy edges
    for (const auto& item : other.callinstToCallGraphEdgesMap)
    {
        const CallICFGNode* cs = item.first;
        for (const CallGraphEdge* edge : item.second)
        {
            PTACallGraphNode* src = getCallGraphNode(edge->getSrcID());
            PTACallGraphNode* dst = getCallGraphNode(edge->getDstID());
            CallSiteID csId = addCallSite(cs, dst->getFunction());

            PTACallGraphEdge* newEdge = new PTACallGraphEdge(src,dst, PTACallGraphEdge::CallRetEdge,csId);
            newEdge->addDirectCallSite(cs);
            addEdge(newEdge);
            callinstToCallGraphEdgesMap[cs].insert(newEdge);
        }
    }

}

/*!
 *  Memory has been cleaned up at GenericGraph
 */
void PTACallGraph::destroy()
{
}

/*!
 *  Whether we have already created this call graph edge
 */
PTACallGraphEdge* PTACallGraph::hasGraphEdge(PTACallGraphNode* src,
        PTACallGraphNode* dst,
        PTACallGraphEdge::CEDGEK kind, CallSiteID csId) const
{
    PTACallGraphEdge edge(src,dst,kind,csId);
    PTACallGraphEdge* outEdge = src->hasOutgoingEdge(&edge);
    PTACallGraphEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}

/*!
 * get PTACallGraph edge via nodes
 */
PTACallGraphEdge* PTACallGraph::getGraphEdge(PTACallGraphNode* src,
        PTACallGraphNode* dst,
        PTACallGraphEdge::CEDGEK kind, CallSiteID)
{
    for (PTACallGraphEdge::CallGraphEdgeSet::iterator iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter)
    {
        PTACallGraphEdge* edge = (*iter);
        if (edge->getEdgeKind() == kind && edge->getDstID() == dst->getId())
            return edge;
    }
    return nullptr;
}


/*!
 * Add indirect call edge to update call graph
 */
void PTACallGraph::addIndirectCallGraphEdge(const CallICFGNode* cs,const SVFFunction* callerFun, const SVFFunction* calleeFun)
{

    PTACallGraphNode* caller = getCallGraphNode(callerFun);
    PTACallGraphNode* callee = getCallGraphNode(calleeFun);

    numOfResolvedIndCallEdge++;

    CallSiteID csId = addCallSite(cs, callee->getFunction());

    if(!hasGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge,csId))
    {
        PTACallGraphEdge* edge = new PTACallGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge, csId);
        edge->addInDirectCallSite(cs);
        addEdge(edge);
        callinstToCallGraphEdgesMap[cs].insert(edge);
    }
}

/*!
 * Get all callsite invoking this callee
 */
void PTACallGraph::getAllCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet)
{
    PTACallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (*it)->directCallsBegin(),
                ecit = (*it)->directCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (*it)->indirectCallsBegin(),
                ecit = (*it)->indirectCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Get direct callsite invoking this callee
 */
void PTACallGraph::getDirCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet)
{
    PTACallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (*it)->directCallsBegin(),
                ecit = (*it)->directCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Get indirect callsite invoking this callee
 */
void PTACallGraph::getIndCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet)
{
    PTACallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (*it)->indirectCallsBegin(),
                ecit = (*it)->indirectCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Issue a warning if the function which has indirect call sites can not be reached from program entry.
 */
void PTACallGraph::verifyCallGraph()
{
    CallEdgeMap::const_iterator it = indirectCallMap.begin();
    CallEdgeMap::const_iterator eit = indirectCallMap.end();
    for (; it != eit; ++it)
    {
        const FunctionSet& targets = it->second;
        if (targets.empty() == false)
        {
            const CallICFGNode* cs = it->first;
            const SVFFunction* func = cs->getCaller();
            if (getCallGraphNode(func)->isReachableFromProgEntry() == false)
                writeWrnMsg(func->getName() + " has indirect call site but not reachable from main");
        }
    }
}

/*!
 * Whether its reachable between two functions
 */
bool PTACallGraph::isReachableBetweenFunctions(const SVFFunction* srcFn, const SVFFunction* dstFn) const
{
    PTACallGraphNode* dstNode = getCallGraphNode(dstFn);

    std::stack<const PTACallGraphNode*> nodeStack;
    NodeBS visitedNodes;
    nodeStack.push(dstNode);
    visitedNodes.set(dstNode->getId());

    while (nodeStack.empty() == false)
    {
        PTACallGraphNode* node = const_cast<PTACallGraphNode*>(nodeStack.top());
        nodeStack.pop();

        if (node->getFunction() == srcFn)
            return true;

        for (CallGraphEdgeConstIter it = node->InEdgeBegin(), eit = node->InEdgeEnd(); it != eit; ++it)
        {
            PTACallGraphEdge* edge = *it;
            if (visitedNodes.test_and_set(edge->getSrcID()))
                nodeStack.push(edge->getSrcNode());
        }
    }

    return false;
}

// author added
static std::set<PTACallGraphNode*> get_neighbors(PTACallGraphNode *node)
{
    std::set<PTACallGraphNode*> res;
    for(const auto &edge : node->getOutEdges())
    {
        // if(edge->isIndirectCallEdge())
        // {
        //     outs() << "found icall edge, src: " << edge->getSrcNode()->getFunction()->getName() << " trg: " << edge->getDstNode()->getFunction()->getName() << std::endl;
        // }
        PTACallGraphNode *trg_node = edge->getDstNode();
        if(res.count(trg_node) > 0)
        {
            continue;
        }
        res.insert(trg_node);
    }
    return res;
}

// author added, this is a naive solution (top-down approach)
template <typename T> std::vector<std::vector<T>> make_disjoint(std::vector<std::vector<T>> &lists) {
    std::vector<T> intersect;
    for (size_t i = 0; i < lists.size(); ++i) 
    {
        for (size_t j = i + 1; j < lists.size(); ++j) 
        {
            set_intersection(lists[i].begin(), lists[i].end(), lists[j].begin(),
                     lists[j].end(), back_inserter(intersect));
        }

        for(size_t i = 0; i < lists.size(); ++i)
        {
            auto &lst = lists[i];
            for( auto &num : intersect)
            {
                auto new_logical_end = std::remove(lst.begin(), lst.end(), num);
                lst.erase(new_logical_end, lst.end());
            }
        }
    }

    if(intersect.size() == 0)
    {
        return lists;
    }

    lists.erase( std::remove_if(lists.begin(), lists.end(), [](std::vector<T> vec){ return vec.size() == 0; }) ); // remove-erase idiom

    std::vector<T> append;
    append.assign(intersect.begin(), intersect.end());
    lists.push_back(append);

    return make_disjoint(lists);
}


// author: refer to https://stackoverflow.com/questions/34693166/what-can-be-the-algorithm-to-find-all-disjoint-sets-from-a-set-of-sets 
// for a fast and elegant solution of this problem (bottom-up approach)
template <typename T> std::vector<std::vector<T>> make_disjoint_advanced(std::vector<std::vector<T>> &lists)
{
    // init the number of nodes
    std::set<PTACallGraphNode *> nodes;
    for( auto lst : lists)
    {
        for(auto node : lst)
        {
            nodes.insert(node);
        }
    }
    std::vector<T> nodes_vec;
    nodes_vec.assign(nodes.begin(), nodes.end());

    int rows = nodes_vec.size();
    int columns = lists.size();

    // init matrix, which encodes the presence of every node within each cluster
    std::vector<std::vector<int>> matrix;
    matrix.reserve(rows);

    for(int i = 0; i < rows; ++i) {
        matrix.push_back(std::vector<int>(columns));
    }

    for(int i = 0; i < rows; ++i)
    {
        for(int j = 0; j < columns; ++j)
        {
            if( std::find(lists[j].begin(), lists[j].end(), nodes_vec[i]) != lists[j].end() )
            {
                matrix[i][j] = 0;
            }
            else
            {
                matrix[i][j] = 1;
            }
        }
    }

    std::map<size_t, std::vector<T>> result_map;
    std::map<size_t, std::vector<int>> md52vec;
    // because columns is very large, we use md5 here, this is not theorotically safe, but it works...
    for(int i = 0; i < rows; ++i)
    {
        std::hash<std::vector<int>> h;
        size_t md5 = h(matrix[i]);

        // sanity check to report hash collision
        if(md52vec.count(md5) != 0 && md52vec[md5] !=  matrix[i])
        {
            outs() << "wtf hash collision occurs\n";
            exit(1); 
        }
        else
        {
            result_map[md5].push_back(nodes_vec[i]);
            md52vec[md5] = matrix[i];
        }
    }

    std::vector<std::vector<T>> result;
    for (const auto &pair : result_map) 
    {
        std::vector<T> cluster = pair.second;
        result.push_back(cluster);
    }

    return result;
}

/*!
 * Dump call graph into dot file
 */
void PTACallGraph::dump(const std::string& filename)
{
    GraphPrinter::WriteGraphToFile(outs(), filename, this);

    std::ofstream dep("deps");
    std::map<SVFFunction *, std::set<SVFFunction *>> dep_info;
    std::vector<std::vector<PTACallGraphNode *>> clusters;

    // const llvm::TargetLibraryInfo *TLI;
    // llvm::LibFunc builtin_funcs;

    outs() << "in function dump()" << std::endl;

    // init a reverse map that maps SVFFunction to llvm::Function
    std::ofstream all_funcs("all_funcs");
    std::map<const SVFFunction *, const SVF::Function *> svf2llvm_funcmap;
    for( auto pair : SVF::LLVMModuleSet::getLLVMModuleSet()->LLVMFunc2SVFFunc)
    {
        auto llvmfunc = pair.first;
        auto svffunc = pair.second;
        if(llvmfunc->isDeclaration() || llvmfunc->isIntrinsic())
        {
            continue;
        }
        svf2llvm_funcmap[svffunc] = llvmfunc;
        all_funcs << svffunc->getName() << std::endl;
    }
    all_funcs.close();

    uint64_t api_cnt = 0, total_trgs = 0; 
    // author: clustering
    for(const auto &mit : this->IDToNodeMap)
    {
        PTACallGraphNode *API_node = mit.second;

        auto API_func = API_node->getFunction();

        // skip external function, and llvm.intrinsic
        if (API_func->isDeclaration() || API_func->isIntrinsic())
        {   
            continue;
        }

        // only focusing on exported functions
        if( !svf2llvm_funcmap[API_func]->hasExternalLinkage() )
        {
            continue;
        }

        std::set<PTACallGraphNode*> visited;
        visited.insert(API_node);
        while(true)
        {
            uint pre_size = visited.size();
            for(auto &node : visited)
            {
                std::set<PTACallGraphNode*> neighbors = get_neighbors(node);
                visited.insert(neighbors.begin(), neighbors.end());
            }

            if (pre_size == visited.size())
            {
                break;
            }
        }

        std::vector<PTACallGraphNode *> current_cluster;
        current_cluster.insert(current_cluster.end(), API_node);
        

        dep << API_func->getName();
        for( auto &reached_node : visited )
        {
            // skip the function itself
            if (reached_node == API_node)
            {
                continue;
            }
            
            // skip llvm intrinsic, but do not skip external function (they might come from assembly?)
            if(reached_node->getFunction()->isIntrinsic()  || reached_node->getFunction()->isDeclaration())
            {
                continue;
            }
            dep << '\t' << reached_node->getFunction()->getName();
            current_cluster.insert(current_cluster.end(), reached_node);
            
        }
        dep << '\n';
        clusters.insert(clusters.end(), current_cluster);
        api_cnt += 1;
        total_trgs += visited.size();
    }
    dep.close();

    outs() << "avg trgs: " << total_trgs / api_cnt << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::vector<PTACallGraphNode *>> disjoint_sets = make_disjoint_advanced(clusters);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    outs() << "\nmake_disjoint takes " << (double)duration.count() / 1000000 << " seconds" << std::endl;
    std::ofstream disjoints("disjoints");
    for(auto vec : disjoint_sets)
    {
        for(auto node : vec)
        {
            disjoints << node->getFunction()->getName() << '\t';
        }
        disjoints << std::endl;
    }
    disjoints.close();


}

void PTACallGraph::view()
{
    SVF::ViewGraph(this, "Call Graph");
}

namespace SVF
{

/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<PTACallGraph*> : public DefaultDOTGraphTraits
{

    typedef PTACallGraphNode NodeType;
    typedef NodeType::iterator ChildIteratorType;
    DOTGraphTraits(bool isSimple = false) :
        DefaultDOTGraphTraits(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(PTACallGraph*)
    {
        return "Call Graph";
    }
    /// Return function name;
    static std::string getNodeLabel(PTACallGraphNode*node, PTACallGraph*)
    {
        return node->toString();
    }

    static std::string getNodeAttributes(PTACallGraphNode*node, PTACallGraph*)
    {
        const SVFFunction* fun = node->getFunction();
        if (!SVFUtil::isExtCall(fun))
        {
            return "shape=box";
        }
        else
            return "shape=Mrecord";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(PTACallGraphNode*, EdgeIter EI,
                                         PTACallGraph*)
    {

        //TODO: mark indirect call of Fork with different color
        PTACallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string color;

        if (edge->getEdgeKind() == PTACallGraphEdge::TDJoinEdge)
        {
            color = "color=green";
        }
        else if (edge->getEdgeKind() == PTACallGraphEdge::TDForkEdge)
        {
            color = "color=blue";
        }
        else
        {
            color = "color=black";
        }
        if (0 != edge->getIndirectCalls().size())
        {
            color = "color=red";
        }
        return color;
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        PTACallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        std::stringstream rawstr(str);
        rawstr << edge->getCallSiteID();

        return rawstr.str();
    }
};
} // End namespace llvm
