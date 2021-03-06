#!/usr/bin/env python3
"""
How to reload this module:
    import hase
    hase.foo()
    # modify source code
    reload(hase)
    # hase should be reloaded now
    hase.foo()

Some Initialization process was copied from the preload.py (of scripting).
Nodes should already have an integer attr named "idepi" representing indirect depth.
    e.g., for v in g.nodes: v.idepi = int(v.idep)
"""
# initialize
try:
    import org.openide.util.Lookup as Lookup
    import org.gephi.layout.api.LayoutController
    LayoutController = Lookup.getDefault().lookup(org.gephi.layout.api.LayoutController)
    import org.gephi.visualization.VizController
    VizController = Lookup.getDefault().lookup(org.gephi.visualization.VizController)
    import org.gephi.layout.plugin.forceAtlas2.ForceAtlas2Builder as ForceAtlas2
    import java.awt.Color as jcolor
    import time
    import org.gephi.scripting.wrappers.GyNode as GyNode
    import org.gephi.scripting.wrappers.GyGraph as GyGraph
except ImportError:
    print("Failed to import Gephi related lib, fallback to cli mode (python3)")
    import json
    from fakegynode import FakeGyNode
    from fakegynode import FakeGyEdge
    import argparse
    import pdb
finally:
    import sys
    if sys.version_info >= (2,7):
        from sys import maxsize as maxint
        from functools import reduce
    else:
        from sys import maxint as maxint

def RunForceAtlas2_nooverlap(iters):
    fa2 = ForceAtlas2().buildLayout()
    LayoutController.setLayout(fa2)
    fa2.setScalingRatio(2.0)
    fa2.setGravity(-2)
    fa2.setAdjustSizes(0)
    LayoutController.executeLayout(iters)
    while (LayoutController.getModel().isRunning()):
        time.sleep(0.2)
    print("%d iters with False AdjustSizes done" % (iters))
    LayoutController.setLayout(fa2)
    fa2.setScalingRatio(2.0)
    fa2.setGravity(-2)
    fa2.setAdjustSizes(1)
    LayoutController.executeLayout(200)
    while (LayoutController.getModel().isRunning()):
        time.sleep(0.2)
    print(" done")

"""
Represent a recordable instruction (valid kinst) in the expression/constraint
graph

@type pygraph: PyGraph
@param pygraph: the PyGraph this recordable instruction is originally from. This
graph will be used to generate the subgraph assuming current instruction is
recorded.

@type gynode: GyNode
@param gynode: We get more information of this instruction from the associated
node in the constraint graph.

@type max_idep: int
@param max_idep: the maximum indirect depth of nodes which still cannot be
concretized

@type rec_nodes: set(node.id)
@param rec_nodes: what symbolic nodes are directly recorded if you record this instruction

@type hidden_nodes: set(node.id)
@param hidden_nodes: what symbolic nodes related to other instruction but still can be
concretized if you record this instruction

@type concretized_nodes: set(node.id)
@param concretized_nodes: what symbolic nodes (not ConstantExpr) can be
concretized after you record this instruction. Note that this list will include
rec_nodes and hidden_nodes, but will not include any ConstantExpr nodes.

Important properties:
    pygraph (PyGraph): see @param above
    subgraph (PyGraph): the PyGraph after concretizing current recordable
        instruction.  This can be used to query indirect depth after recording
    kinst (str): a global unique identifier of this instruction
    width (int): the width of the result(destination register) of this
        instruction
    freq (int): how many times this instruction got executed in the entire trace
"""
class RecordableInst(object):
    SUBGRAPH = False
    def __init__(self, pygraph, gynode, rec_nodes, hidden_nodes,
            concretized_nodes):
        self.pygraph = pygraph
        self.kinst = gynode.kinst
        self.width = int(gynode.width)
        self.freq = int(gynode.freq)
        self.ispointer = True if gynode.ispointer == "true" else False
        self.rec_nodes = rec_nodes
        self.hidden_nodes = hidden_nodes
        self.concretized_nodes = concretized_nodes
        # heuristics related property
        self.nodeReduction = len(self.concretized_nodes)
        self.coverageScore = sum([
            float(self.pygraph.id_map[nid].width) / 8 * \
            (1+self.pygraph.idep_map[nid]) for nid in self.concretized_nodes
            ])
        self.recordSize = self.freq * 8 # 8B (64b), not self.width,
                                        # because of ptwrite limitation
        self.recordSizeNONPT = self.freq * self.width / 8
        self.coverageScoreFreq = self.coverageScore / self.recordSize
        if RecordableInst.SUBGRAPH:
            subgraph = pygraph.buildFromPyGraph(self.pygraph, concretized_nodes)
            self.max_idep = subgraph.max_idep()
            self.remainScore = sum([float(n.width) / 8 * (1+subgraph.idep_map[n.id])
                for n in subgraph.gynodes])
        else:
            self.max_idep = 0
            self.remainScore = 0
        # sanity check
        if not isinstance(rec_nodes, set):
            raise RuntimeError("rec_nodes is not a set ")
        if not isinstance(hidden_nodes, set):
            raise RuntimeError("hidden_nodes is not a set ")
        if not isinstance(concretized_nodes, set):
            raise RuntimeError("concretized_nodes is not a set ")
        if self.width == 0:
            raise RuntimeError("Zero Width instruction")

    def __str__(self):
        return "kinst: %s, width: %d, freq: %d, %d nodes recorded, %d nodes hidden,"\
                "%d nodes concretized" % (self.kinst, self.width, self.freq,\
                len(self.rec_nodes), len(self.hidden_nodes),
                len(self.concretized_nodes))

    def __repr__(self):
        return self.__str__()

    """
    Create easy to check dict
    @type recinsts: List(RecordableInst)
    @param recinsts: a list of RecordableInstructions

    @rtype Dict(str->RecordableInst)
    @return: a dict maps kinst string to associated RecordableInst object
    """
    @staticmethod
    def getStrDict(recinsts):
        d = {}
        for r in recinsts:
            d[r.kinst] = r
        return d
"""
Check if the kinst of a GyNode is valid
node(GyNode)
"""
def isKInstValid(node):
    return (node.kinst is not None) and \
            (len(node.kinst) > 0) and \
            node.kinst != 'N/A'

"""
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!IMPORTANT!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
The GyNode type (python wrapper of a Java object) is buggy in terms of Set
operations.
E.g. Bellow python code segment will give you "False"
```
l = [ x for x in g.nodes ]
l[0] in g.nodes
```
"""
class PyGraph(object):
    # If we assume PTWRITE limitation (minimum record 8B)
    PTWRITE = True
    ALLOWPTR = False

    """
    @type gygraph: GyGraph
    @param gygraph: the origin graph available in the python script plugin ('g')

    @rtype: PyGraph
    @return: A new PyGraph built from the given GyGraph
    """
    @classmethod
    def buildFromGyGraph(cls, gygraph):
        if isinstance(gygraph, GyGraph):
            # Here we filter out the "dummy nodes" which are used to scale edge
            # width in Gephi
            gynodes = set([n for n in gygraph.nodes if n.kind is not None])
            gyedges = set([e for e in gygraph.edges if e.source.kind is not None
                and e.target.kind is not None])
            return PyGraph(gynodes, gyedges)
        else:
            return None

    """
    @type pygraph: PyGraph
    @param pygraph: the base graph you are working on
    @type deleted_nodes: set(node_id)
    @param deleted_nodes: a set of node_id you want to delete from pygraph
    """
    @classmethod
    def buildFromPyGraph(cls, pygraph, deleted_nodes):
        if isinstance(pygraph, PyGraph) and isinstance(deleted_nodes, set):
            subgynodes = set([ n for n in pygraph.gynodes if n.id not in
                deleted_nodes])
            subgyedges = set([e for e in pygraph.gyedges if e.source.id not in
                deleted_nodes and e.target.id not in deleted_nodes])
            return PyGraph(subgynodes, subgyedges, pygraph.nodePostDom)
        else:
            return None

    """
    @type graphdict: Dict, The json graph seems like:
    {
        "edges": [
            {
                "source": "94530732880448",
                "target": "94530732878272",
                "weight": 1.0
            }, ...
        ]
        "nodes": {
            "94530648900736": {
                "Category": "N"/"Q"/"C",
                "DbgInfo": "N/A",
                "Freq": 0,
                "IDep": 0,
                "IsPointer": "false",
                "KInst": "N/A",
                "Kind": 3,
                "Width": 8,
                "label": "model_version[0]"
            }, ...
        }
    }
    """
    @classmethod
    def buildFromPyDict(cls, graphdict):
        nodes = graphdict["nodes"]
        edges = graphdict["edges"]
        fakegynodes = {}
        for nid, nprop in nodes.items():
            fakegynodes[nid] = FakeGyNode(nid, nprop)
        GyNodeSet = set(fakegynodes.values())
        GyEdgeSet = set([FakeGyEdge(fakegynodes[e["source"]],
        fakegynodes[e["target"]], e) for e in edges])
        return PyGraph(GyNodeSet, GyEdgeSet)

    """
    @type GyNodeSet: set(GyNode)
    @param GyNodeSet: set of GyNodes you want build graph from
    @type GyEdgeSet: set(GyEdge)
    @param GyEdgeSet: set of GyEdges you want to build graph from
    """
    def __init__(self, GyNodeSet, GyEdgeSet, nodePostDom = None):
        self.gynodes = GyNodeSet
        self.gyedges = GyEdgeSet

        # edge map node.id -> set of edges starting from node
        self.edges = {}
        # reverse edge map, node.id -> set of edges ending in node
        self.redges = {}
        # map node.id -> node
        self.id_map = {}
        # node.id -> topological id (0..|V|-1)
        # note that the klee expression graph looks like
        # [operator] -> [operand0]
        #            -> [operand1]
        # so the edge represents [dependant] -> [dependency]
        # The assigned topological id: [dependant] > [dependency]
        # aka [result] > [operands]
        self.topological_map = None
        # @type: List(GyNode)
        # list of nodes from small topo id to large topo id
        # (from high indirect depth to low indirect depth)
        self.all_nodes_topo_order = None

        # @type: Dict(str->set(nid))
        self.kinst2nodes = None
        # @type: Dict(nid->set(nid))
        # The post dominator of every kinst
        self.nodePostDom = nodePostDom
        # @type: Dict(node_id->int)
        # map a node_id to indirect depth
        self.idep_map = None

        # innodes: no in edges, outnodes: no out edges
        # @type: set(node_id)
        self.innodes = set()
        self.outnodes = set()
        for e in self.gyedges:
            self.edges.setdefault(e.source.id, set()).add(e)
            self.redges.setdefault(e.target.id, set()).add(e)
        for n in self.gynodes:
            self.id_map[n.id] = n
            if n.id not in self.redges:
                self.innodes.add(n.id)
            if n.id not in self.edges:
                self.outnodes.add(n.id)
        self.topological_sort()
        self.build_kinst2nodes()
        if self.nodePostDom is None:
            self.build_nodePostDom()
        self.all_nodes_topo_order = sorted(self.gynodes,
                key=lambda n: self.topological_map[n.id])
        self.calculate_idep()
        # Cache MustConcretize results
        # Dict(nid->set(nid))
        self.mustconcretize_cache = {}

    """
    Perform topological sort and store the result in self.topological_map
    """
    def topological_sort(self):
        topological_cnt = 0
        self.topological_map = {}
        # set of node.id
        visited_nodes = set()
        # list of node
        worklist = []
        for node in self.gynodes:
            if node.id not in visited_nodes:
                worklist.append(node)
                while len(worklist) > 0:
                    n = worklist[-1]
                    if n.id not in visited_nodes:
                        visited_nodes.add(n.id)
                        for e in self.edges.get(n.id, []):
                            if e.target.id not in visited_nodes:
                                worklist.append(e.target)
                    else:
                        if n.id not in self.topological_map:
                            self.topological_map[n.id] = topological_cnt
                            topological_cnt = topological_cnt + 1
                        worklist.pop()

    """
    Dependency: all_nodes_topo_order
    calculate indirect depth of all nodes.
    Will traverse nodes in the reverse topological order.
    @rtype: None
    """
    def calculate_idep(self):
        self.idep_map = {}
        for node in reversed(self.all_nodes_topo_order):
            if node.id not in self.redges:
                self.idep_map[node.id] = 0
            else:
                possible_idep = []
                for e in self.redges[node.id]:
                    parent_idep = self.idep_map[e.source.id]
                    if e.weight == 1.0:
                        possible_idep.append(parent_idep)
                    elif e.weight == 1.5:
                        possible_idep.append(parent_idep + 1)
                    else:
                        print("edge: " + e + " has invalid weight")
                        raise RuntimeError("Invalid edge weight")
                self.idep_map[node.id] = max(possible_idep)

    """
    Dependency: calculate_idep
    @rtype: int
    @return: the max indirect depth in the current graph
    """
    def max_idep(self):
        if len(self.idep_map) > 0:
            return max(self.idep_map.values())
        else:
            return 0

    """
    build the mapping from recordable instructions to all associated nodes in
    the constraint graph
    """
    def build_kinst2nodes(self):
        self.kinst2nodes = {}
        for node in self.gynodes:
            if isKInstValid(node):
                self.kinst2nodes.setdefault(node.kinst, set()).add(node.id)

    """
    Depedency: id_map
    """
    def build_nodePostDom(self):
        self.nodePostDom = {}
        worklist = []
        all_nids = frozenset(self.id_map.keys())
        for n in self.gynodes:
            if n.id in self.edges:
                self.nodePostDom[n.id] = all_nids
            else:
                self.nodePostDom[n.id] = frozenset()
                worklist.append(n.id)
        while len(worklist) > 0:
            newworklist = []
            for changed_nid in worklist:
                for e in self.redges.get(changed_nid, set()):
                    n = e.source
                    edges = self.edges.get(n.id, set())
                    successors = set([e.target.id for e in edges])
                    nsuccessor = len(successors)
                    assert(nsuccessor > 0)
                    if nsuccessor == 1:
                        single_succ = list(successors)[0]
                        newPostDom = self.nodePostDom[single_succ] | \
                                frozenset([single_succ])
                    else:
                        succPostDom = [self.nodePostDom[succ] for succ in
                                successors]
                        newPostDom = reduce(frozenset.intersection,
                                succPostDom)
                    if newPostDom != self.nodePostDom[n.id]:
                        self.nodePostDom[n.id] = newPostDom
                        newworklist.append(n.id)
            worklist = newworklist

    """
    Dependency: topological_map
    Dependency: kinst2nodes
    Analyze all recordable nodes on the constraint graph
    Optimization:
    Given two recordable nodes n1 and n2, if only concretize n1 will also
    concretize n2, then n2 will be considered unnecessary to record and only n1
    will be reported (if no other recordable nodes can concretize n1).
    @type recinsts: List(RecordableInst)
    @param recinsts: list of recordable instructions we already decided to
    record

    @rtype: List(List(RecordableInst))
    @return: a list all beneficial recordable instructions, each entry is a list
    of interesting recordable instructions accumulated so far.
    note that each outer list entry represents a sequence of accumulated
    recordable instructions. And each inner list entry represent a recordable
    instruction. All outer list entries are lists of the same length x. And the
    first x-1 inner list entries of each outer list entries are same. Only the
    last inner list entry (newly added interesting RecordableInst) differs,
    which we should sort as key.
    instruction)
    """
    def analyze_recordable(self, recinsts=[]):
        # checked_kinst_set contains nodes no long require analysis
        # @type Set(node.id)
        checked_kinst_set = set()

        # concretized_set contains all nodes concretized by either ConstantExpr
        # or the already recorded instructions
        # @type Set(node.id)
        concretized_set = set()
        # populate data structures using input recinsts
        for recinst in recinsts:
            for nid in recinst.rec_nodes:
                concretized_set.add(nid)
                checked_kinst_set.add(nid)
            for nid in recinst.hidden_nodes:
                checked_kinst_set.add(nid)

        # @type: List(List(RecordableInst))
        result = []
        # Pre Process
        for node in self.all_nodes_topo_order:
            # find the closure of given concretized_set
            if node.id in self.edges and \
               all([(e.target.kind == "0") or (e.target.id in concretized_set) \
                    for e in self.edges[node.id]]):
                concretized_set.add(node.id)
        # union the sets of concretized nodes from multiple recordable
        # instructions
        in_all_concretized_nodes = set()
        for recinst in recinsts:
            for nid in recinst.concretized_nodes:
                in_all_concretized_nodes.add(nid)

        if concretized_set != in_all_concretized_nodes:
            print("Warn: input graph is not simplified, "
                  "dangling constant nodes detected")

        for seqid, n in enumerate(self.all_nodes_topo_order):
            if (isKInstValid(n)) and (n.id not in checked_kinst_set):
                for nid in self.kinst2nodes[n.kinst]:
                    checked_kinst_set.add(nid)
                newRecordableInst = self.analyze_single_kinst(n.kinst,
                        concretized_set, seqid)
                result.append(recinsts + [newRecordableInst])
        return result

    """
    @type kinst: str
    @param kinst: The instruction identifier I want to record
    @type concretized_set: set(node.id)
    @param concretize_set: contains nodes assumed to be concretized
    @type hint_topo: int
    @param hint_topo: A hint of the position from which I should traverse the
        graph in topological order
    @rtype RecordableInst
    @return construct a new RecordableInst based on a given kinst and already
    concretized nodes
    """
    def analyze_single_kinst(self, kinst, concretized_set, hint_topo = -1):
        local_concretized_set = concretized_set.copy()
        hidden_nodes = set()
        for nid in self.kinst2nodes[kinst]:
            local_concretized_set.add(nid)
        n = self.id_map[list(self.kinst2nodes[kinst])[0]]
        for node in self.all_nodes_topo_order[hint_topo+1:]:
            # skip ConstantExpr and nodes without out edges
            # only consider nontrivial intermediate nodes
            if (node.kind != "0") and (node.id in self.edges) and \
            (node.id not in local_concretized_set):
                const_nodes = [e.target.id for e in self.edges[node.id]
                        if e.target.kind == "0"]
                known_symbolic_nodes = [e.target.id for e in
                        self.edges[node.id] if e.target.id in
                        local_concretized_set]
                # this node can be concretized
                if len(const_nodes) + len(known_symbolic_nodes) == \
                len(self.edges[node.id]):
                    local_concretized_set.add(node.id)
                    # this node is hidden if:
                    # 1) it can be concretized here
                    # 2) it has a valid KInst
                    if len(known_symbolic_nodes) > 0 and isKInstValid(node):
                        hidden_nodes.add(node.id)
                if len(const_nodes) + len(known_symbolic_nodes) > \
                len(self.edges[node.id]):
                    raise RuntimeError("sum of out edges wrong")
        return RecordableInst(self, n, self.kinst2nodes[n.kinst], hidden_nodes,
                local_concretized_set - concretized_set)

    """
    @rtype: int
    @return: total bytes need to be recorded for the given instruction list,
        assuming PT limitation (8B minimum recording unit)
    """
    @classmethod
    def recordSize(cls, recinsts):
        return sum([recinst.recordSize for recinst in recinsts])

    """
    @rtype: float
    @return: total bytes need to be recorded for the given instruction list,
        the real width of each instruction is considered (not assuming PT
        limitation)
    """
    @classmethod
    def recordSizeNONPT(cls, recinsts):
        return sum([recinst.recordSizeNONPT for recinst in recinsts])

    """
    @rtype: float
    @return: the heuristic score of this Recordable Instruction. expression
    width and indirect depth are considered
    (higher is better)
    """
    @classmethod
    def coverageScore(cls, recinsts):
        return sum([recinst.coverageScore for recinst in recinsts])
    """
    @type recinstsL: List(List(RecordableInst))
    @param recinstsL: list of list of recordable instructions from
    analyze_recordable()

    @rtype: List(List(RecordableInst))
    @return: sorted list, sorted according to coverageScore
    """
    @classmethod
    def sortRecInstsbyCoverageScore(cls, recinstsL):
        return sorted(recinstsL, key=lambda recinsts:
                cls.coverageScore(recinsts))

    """
    @rtype: float
    @return: Heuristics take instruction frequency into consideration
    (higher is better)
    """
    @classmethod
    def coverageScoreFreq(cls, recinsts):
        return cls.coverageScore(recinsts) / cls.recordSize(recinsts)

    """
    same as above
    """
    @classmethod
    def sortRecInstsbyCoverageScoreFreq(cls, recinstsL):
        return sorted(recinstsL, key=lambda recinsts:
                cls.coverageScoreFreq(recinsts))

    @classmethod
    def nodeReduction(cls, recinsts):
        return sum([recinst.nodeReduction for recinst in recinsts])

    @classmethod
    def sortRecInstsbyNodeReduction(cls, recinstsL):
        return sorted(recinstsL, key=lambda recinsts:
                cls.nodeReduction(recinsts))
    @classmethod
    def nodeReductionPerByte(cls, recinsts):
        return cls.nodeReduction(recinsts) / cls.recordSize(recinsts)
    @classmethod
    def sortRecInstsbyNodeReductionPerByte(cls, recinstsL):
        return sorted(recinstsL, key=lambda recinsts:
                cls.nodeReductionPerByte(recinsts))

    """
    @rtype float
    @return Heuristics only consider the graph after give instructions are
    recorded
    (lower is better)
    """
    @classmethod
    def remainScore(cls, recinsts):
        return recinsts[-1].remainScore
    @classmethod
    def sortRecInstbyRemainScoreFreq(cls, recinstsL):
        return sorted(recinstsL, key=lambda recinsts:
                (recinsts[-1].max_idep, cls.recordSize(recinsts),
                    cls.remainScore(recinsts)))

    @classmethod
    def hasPointer(cls, recinsts):
        return any([recinst.ispointer for recinst in recinsts])

    """
    @type recinsts: List(RecordableInst)
    @param recinsts: list of recordable instructions selected in one or more
    iterations to record. These instructions will be considered as one entity.

    @rtype: str
    @return: A string represents important information of recorded nodes in a
    RecordableInst
    """
    def getRecInstsInfo(self, recinsts):
        concretized_nodes = set()
        max_unconcretized_depth = 0
        for recinst in recinsts:
            if concretized_nodes & recinst.concretized_nodes:
                raise RuntimeError(
                "recordable instruction list has concretized_nodes overlap")
            concretized_nodes |=  recinst.concretized_nodes
        msgstring = ""
        for seq, recinst in enumerate(recinsts):
            msgstring += "Rec[%d]: " % seq
            msgstring += "[Ptr]" if recinst.ispointer else "[Val]"
            msgstring += recinst.__str__() + "\t"
            msgstring += "max idep %d -> %d\n" % (recinst.pygraph.max_idep(),
                    recinst.max_idep)
            msgstring += 'rec_nodes_label: ' + \
                    ', '.join([self.id_map[nid].label for nid in
                        list(recinst.rec_nodes)[:10]])
            if len(recinst.rec_nodes) > 10:
                msgstring += ", ..."
            msgstring += "\n"

        msgstring += "CoverageScore=%f, " % self.coverageScore(recinsts) +\
                "CoverageFreqScore=%f, " % self.coverageScoreFreq(recinsts) +\
                "RemainScore=%f, " % self.remainScore(recinsts) +\
                "RecordSize=%d, " % self.recordSize(recinsts) +\
                "RecordSizeNOPT=%d\n" % self.recordSizeNONPT(recinsts)
        msgstring += "Total: "
        percent_concretized = \
        (float(len(concretized_nodes))/len(self.gynodes)*100)
        msgstring += '%d(%f%%) nodes concretized.' % \
                (len(concretized_nodes), percent_concretized)
        return msgstring

    """
    Print all recordable instruction candiadates in the given order
    @type recinstsL: List(List(RecordableInst))
    @param recinstsL: list of list of recordable instructions from
    analyze_recordable() or sorted by some heuristics.

    @rtype: None
    """
    def printCandidateRecInstsInfo(self, recinstsL):
        for seq, recinsts in enumerate(recinstsL):
            print(("###(%4d)###\n" % seq) + self.getRecInstsInfo(recinsts) + '\n')

    def ColorCSet(self, nodes_id_set):
        s = self.concretize_set(nodes_id_set)
        self.MarkNodesWhiteByID(s)
        self.MarkNodesRedByID(nodes_id_set)
        return s

    def SelectNodesByID(self, nodes):
        VizController.selectionManager.selectNodes([self.id_map[vid].getNode()
            for vid in nodes])

    def SelectNodes(self, nodes):
        VizController.selectionManager.selectNodes([v.getNode() for v in nodes])

    def ColorNodesByID(self, nodes_id, color):
        for nid in nodes_id:
            self.id_map[nid].color = color

    def ColorAllNodes(self, color):
        for n in self.gynodes:
            n.color = color;

    def MarkNodesWhiteByID(self, nodes_id):
        self.ColorNodesByID(nodes_id, jcolor.white)

    def MarkNodesRedByID(self, nodes_id):
        self.ColorNodesByID(nodes_id, jcolor.red)

    """
    Visualize a RecordableInst on the graph
    @type recinsts: List(RecordableInst)
    @param recinsts: a recording configuration
    """
    def VisualizeRecordableInst(self, recinsts):
        colorednodes = set()
        for recinst in recinsts:
            # sanity check: should not color the same node twice
            overlap = recinst.concretized_nodes & colorednodes
            if len(overlap) != 0:
                print("The following nodes will be colored twice:")
                for nid in overlap:
                    print("id: %s, label %s\n", nid, self.id_map[nid].label)
                raise RuntimeError("Color the same node twice")
            # end of sanity check
            # color 3 types of nodes in different colors:
            # 1) the nodes will be directly recorded
            # 2) the nodes will be hidden (hidden is defined in func
            # analyze_recordable)
            # 3) other nodes
            non_hidden_nodes = recinst.concretized_nodes -\
            set(recinst.rec_nodes) - set(recinst.hidden_nodes)
            self.MarkNodesRedByID(recinst.rec_nodes)
            self.MarkNodesWhiteByID(non_hidden_nodes)
            self.ColorNodesByID(recinst.hidden_nodes, jcolor.green)
            colorednodes |= recinst.concretized_nodes

    """
    Filter list of RecordableInst which can concretize the given node.
    This is helpful when you encounter a symbolic external function call.
    """
    def FilterMustRecordQuery(self, recinstsL, nid):
        filtered = []
        for recinsts in recinstsL:
            for recinst in recinsts:
                if nid in recinst.concretized_nodes:
                    filtered.append(recinsts)
                break
        return filtered

    """
    @rtype: int
    @return: in bytes
    """
    def GetKInstSetRecordingSize(self, kinstset):
        return sum([self.GetNodeRecordingSize(n) for n in
            [self.id_map[list(self.kinst2nodes[k])[0]] for k in kinstset]
            ])

    """
    @type n: GyNode
    @param n: node to reason about
    @rtype: float/int
    @return: in bytes
    """
    @classmethod
    def GetNodeRecordingSize(cls, n):
        if cls.PTWRITE:
            return int(n.freq) * 8
        else:
            return int(n.freq)*int(n.width) / 8

    """
    @type nids: List(nid)
    @param nids: nodes you want to record
    @rtype: set(str)
    @return: a set of kinst identifiers
    """
    def GetKInstSetFromNids(self, nids):
        return set([self.id_map[nid].kinst for nid in nids])
    """
    @type nid: str
    @param nid: The string id of the node you must concretize
    @type minbytes: int
    @rtype Set(nid)
    @return A set of nids representing the nodes costing the least amount of
    data to record, if you want to concretize a single given node nid.
    Note: To get a set of instruction identifiers instead, you should call
    GetKInstSetFromNids.
    """
    def MustConcretize(self, nid):
        # nid is not in the constraint graph
        # it could already concretized, or not a valid node
        # In no matter which case, we do not need to record anything
        if nid not in self.id_map:
            return set()
        # DFS
        worklist = [nid]
        visited_nid = set()
        while len(worklist) > 0:
            wnid = worklist[-1]
            if wnid in visited_nid:
                worklist.pop()
                # collect and compare with children
                n = self.id_map[wnid]
                if isKInstValid(n) and (self.ALLOWPTR or n.ispointer == "false"):
                    self_bytes = self.GetNodeRecordingSize(n)
                else:
                    self_bytes = maxint
                child_nidset = set()
                for e in self.edges.get(wnid, set()):
                    child_nidset |= self.mustconcretize_cache[e.target.id]
                child_nidset_dedup = set([cnid for cnid in child_nidset if
                    len(self.nodePostDom[cnid]) == 0 or
                    not self.nodePostDom[cnid].issubset(child_nidset)])
                #if child_nidset_dedup != child_nidset:
                #    print("Dedup, removed %s" %
                #            ', '.join(child_nidset - child_nidset_dedup))
                child_kinstset = self.GetKInstSetFromNids(child_nidset_dedup)
                # if any child is non-recordable, then we use an empty
                # child_kinstset, which leads to zero bytes to record.
                # zero bytes to record is identified to be non-recordable
                if 'N/A' in child_kinstset:
                    child_kinstset = set()
                child_bytes = self.GetKInstSetRecordingSize(child_kinstset)
                if child_bytes > 0 and child_bytes <= self_bytes:
                    self.mustconcretize_cache[wnid] = child_nidset_dedup
                    #self.id_map[wnid].color = jcolor.pink
                elif n.kind == 0: # this is a constant node, already concretized
                    self.mustconcretize_cache[wnid] = set()
                    #self.id_map[wnid].color = jcolor.orange
                else:
                    self.mustconcretize_cache[wnid] = set([n.id])
                    #self.id_map[wnid].color = jcolor.blue
            else:
                visited_nid.add(wnid)
                # recursive add childs to worklist
                for e in self.edges.get(wnid, set()):
                    if e.target.id not in self.mustconcretize_cache:
                        worklist.append(e.target.id)
        return self.mustconcretize_cache[nid]

    """
    @type arraynames: set of string
    @param arraynames: the arrays, upon which you want to get rid of all
    symbolic index access.
    @param indep_thres: int
    @rtype: list of RecordableInst
    @return: the list of RecordableInst to concretize all symbolic indirect
        access (Read and Write/UN)
    """
    def UpdateListConcretize(self, arraynames, indep_thres):
        # initial_strategy is the union of the best strategy of concretize
        # nodes individually
        initial_strategy = set()
        visited_nids = set()
        for n in self.all_nodes_topo_order:
            if str(n.kind) == "UN" and n.root.split("[")[0] in arraynames:
                for e in self.edges[n.id]:
                    if e.weight == 1.5 and e.target.id not in visited_nids and \
                    self.idep_map[e.target.id] >= indep_thres:
                        visited_nids.add(e.target.id)
                        initial_strategy |= self.MustConcretize(e.target.id)
            if str(n.kind) == "3" and n.root.split("[")[0] in arraynames: # ReadExpr
                readnode = n
                for re in self.edges[readnode.id]:
                    if re.weight == 1.5 and \
                            re.target.id not in visited_nids and \
                            self.idep_map[re.target.id] >= indep_thres:
                        visited_nids.add(re.target.id)
                        initial_strategy |= self.MustConcretize(re.target.id)
        PyGraph.ALLOWPTR = False
        noptr_strategy = self.recursiveOptimizeRecNids(initial_strategy)
        PyGraph.ALLOWPTR = True
        # we now allow ptr, need to flush all previous results
        self.mustconcretize_cache.clear()
        ptr_strategy = self.recursiveOptimizeRecNids(noptr_strategy)
        PyGraph.ALLOWPTR = False
        # newRIlist, subh = self.recursiveOptimizeRecKInstL(kinstset)
        return ptr_strategy

    """
    @type kinsts: List of str
    @param kinsts: a list of kinst to be recorded in the given order.
    @rtype: tuple(List of RecordableInst, PyGraph)
    @return (A list of analyzed RecordableInst, the PyGraph after recording
    given kinsts)
    """
    def buildRecKInstL(self, kinsts):
        subh = self
        kinst_list = []
        concretized_set = set()
        for kinst in kinsts:
            if kinst in subh.kinst2nodes:
                newRI = subh.analyze_single_kinst(kinst, concretized_set)
                subh = subh.buildFromPyGraph(subh, newRI.concretized_nodes)
                concretized_set |= newRI.concretized_nodes
                kinst_list.append(newRI)
        return (kinst_list, subh)

    def recursiveOptimizeRecNids(self, _nids):
        if len(_nids) == 0:
            return ([], self)
        # since multiple nodes can be mapped to the same kinst,
        # we have to make sure when we want to replace one kinst with other
        # kinsts, the cost of concretizing all nodes we are interested in
        # should be reduced

        # kinst2nids tracks when we select a kinst to record, which nodes are we
        # really care about.
        # Dict(kinst -> set(nid))
        kinst2nids = {}
        for nid in _nids:
            kinst2nids.setdefault(self.id_map[nid].kinst, set()).add(nid)
        kinsts = list(kinst2nids.keys())
        while True:
            changed = False
            start = kinsts[0]
            while True:
                k = kinsts.pop(0)
                #print("Reasoning %s based on %s" % (k, ', '.join(kinsts)))
                kl, subh = self.buildRecKInstL(kinsts)
                target_nids = kinst2nids[k]
                RecNids = set()
                for nid in target_nids:
                    SingleNodeNewRecNids = subh.MustConcretize(nid)
                    #print("nid:%s, newRecNids: %s" % (nid,
                    #    ','.join(SingleNodeNewRecNids)))
                    RecNids |= SingleNodeNewRecNids
                # FIXME: fix here RecNids could have "N/A" kinst by running
                # MustConcretize on the initial strategy
                newKInstSet = self.GetKInstSetFromNids(RecNids)
                newRecCost = self.GetKInstSetRecordingSize(newKInstSet)
                oldRecCost = self.GetKInstSetRecordingSize([k])
                # take any single element from a set
                # said to be the best practice in python3.X
                for anynid in kinst2nids[k]: break
                oldRecIsPointer = (self.id_map[anynid].ispointer == "true")
                #print("newRecCost %d, oldRecCost %d, newKInstSet %s" % (
                #    newRecCost, oldRecCost, ','.join(newKInstSet)))
                # When to replace old recording strategy with new ones?
                # First, the old KInst has not to remain in new strategy
                # Second, one of the following conditions should be true:
                #    1. new cost is cheaper than old cost
                #    2. new cost is the same as the old cost but new strategy
                #    only records a single instruction (I forgot why I need
                #    this)
                #    3. old strategy is a pointer but pointer recording is
                #    disallowed. Note that the above MustConcretize should
                #    guarantee the new strategy does not record pointer if
                #    disallowed.
                if (k not in newKInstSet) and (
                    (newRecCost < oldRecCost) or \
                    (newRecCost == oldRecCost and len(newKInstSet) == 1) or \
                    ((not PyGraph.ALLOWPTR) and oldRecIsPointer)
                    ):
                    changed = True
                    # replace old kinst with the new ones
                    del kinst2nids[k]
                    #print("Replace %s with %s" % (k, ','.join(newKInstSet)))
                    for nid in RecNids:
                        kinst2nids.setdefault(self.id_map[nid].kinst,
                                set()).add(nid)
                    kinsts.extend(newKInstSet)
                    break
                else:
                    kinsts.append(k)
                if kinsts[0] == start:
                    break
            if not changed:
                break
        finalNids = set()
        for key, value in kinst2nids.items():
            finalNids |= value
        return finalNids


class HaseUtils(object):
    def __init__(self, globals_ref):
        self.globals = globals_ref
        self.g = self.globals['g']

        node_attrs = self.g.getNodeAttributes()
        self.attr_idepi = node_attrs['idepi']
        self.run_layout = RunForceAtlas2_nooverlap

        # important data structures
        self.maxIDep = max([v.idepi for v in self.g.nodes])
        self.idep_subg = []
        # a metric to sort root nodes from the same layer:
        # for each node n: metric(n) = sum(src.idepi if src->n has an index edge)
        # map from node.id -> score
        self.idep_index_score = {}
        self.all_root_nodes = set() # root nodes of every indirect depth layer

        # initialize important data structures
        for i in range(self.maxIDep+1):
            self.idep_subg.append(self.g.filter(self.attr_idepi==i))
        # all first level nodes is considered root
        for v in self.g.nodes:
            if v.idepi == 0:
                self.all_root_nodes.add(v.id)
                self.idep_index_score[v.id] = 0
        # nodes pointed by cross-layer edges but not by intra-layer edges are roots
        # nodes pointed by cross-layer edges will be enlarged
        for e in self.g.edges:
            if e.weight==1.5:
                if e.target.idepi == e.source.idepi + 1:
                    self.all_root_nodes.add(e.target.id)
                self.idep_index_score[e.target.id] = self.idep_index_score.get(e.target.id, 0) + e.source.idepi
        for e in self.g.edges:
            if (e.target.idepi == e.source.idepi) and \
                    (e.target.id in self.all_root_nodes):
                        self.all_root_nodes.remove(e.target.id)
                        del self.idep_index_score[e.target.id]
        self.resize_root_nodes()
        self.idep_roots = []

        """
        0 level, root nodes have special definition on this level:
          1. root nodes are nodes without indegree
          2. root nodes are sorted based on their outdegree
        """
        self.idep_roots.append(
                sorted(
                    [v for v in self.idep_subg[0].nodes if v.indegree == 0 and \
                        v.id in self.all_root_nodes],
                    key=lambda v: v.outdegree
                )
        )
        """
        non-zero level
          1. root nodes at level n are nodes pointed from level n-1
          2. root nodes are sorted based on their index_score (defined above)
        """
        for subg in self.idep_subg[1:]:
            self.idep_roots.append(
                    sorted(
                        [v for v in subg.nodes if v.id in self.all_root_nodes],
                        key=lambda v: str(self.idep_index_score.get(v.id, 0)) + v.label
                    )
            )

    """
    Focus on the n-th level of the dependency graph
    this function will:
      1. layout the root nodes of that level
      2. settle the root nodes
      3. make only current level visible
    """
    def focus_idep(self, idep):
        self.layout_idep_root(idep)
        self.lock_idep(idep)
        self.visible_idep(idep)

    def resize_root_nodes(self):
        for n in self.g.nodes:
            if n.id in self.all_root_nodes:
                n.size = 17
            else:
                n.size = 10
    """
    Layout the root nodes of a given level
    Will evenly spread roots (sorted) along Y-axis on the left most border
    (min x-axis) of that level
    """
    def layout_idep_root(self, idep, cols=1, xstep=50):
        subg = self.idep_subg[idep]
        roots = self.idep_roots[idep]
        layout_ymin = min([v.y for v in subg.nodes])
        layout_ymax = max([v.y for v in subg.nodes])
        layout_xmin = min([v.x for v in subg.nodes])
        ystep = (layout_ymax - layout_ymin) / (len(roots)//cols+1)
        for (i,r) in enumerate(roots):
            row = i//cols;
            col = i%cols;
            r.x = layout_xmin + col * xstep
            r.y = layout_ymin + (row+1) * ystep

    """
    If the spread roots are too close to each other, you may want to scale them
    along Y-axis.
    """
    def yexpand_idep_root(self, idep, ratio):
        for v in self.idep_roots[idep]:
            v.y *= ratio

    """
    Mark nodes from given level fixed (position will not change when running
    layout)
    """
    def lock_idep(self, idep):
        roots = self.idep_roots[idep]
        for r in roots:
            r.fixed = True

    """
    Mark nodes from given level non-fixed again.
    """
    def free_idep(self, idep):
        for r in self.idep_roots[idep]:
            r.fixed = False

    """
    Make nodes from given level the only nodes visible on workspace
    """
    def visible_idep(self, idep):
        self.setVisible(self.idep_subg[idep])

    """
    Move nodes with indirect depth >= `start_idep` to the right by distance `dist`
    """
    def move_right(self, start_idep, dist):
        subg = self.g.filter(self.attr_idepi >= start_idep)
        for v in subg.nodes:
            v.x += dist
    def move_right_auto(self, start_idep):
        if start_idep > 0:
            subg = self.g.filter(self.attr_idepi >= start_idep)
            cur_minx = min([v.x for v in subg.nodes])
            prev_maxx = max([v.x for v in self.idep_subg[start_idep-1].nodes])
            self.move_right(start_idep, prev_maxx + 100 - cur_minx)

    def auto_layout_all(self, iters=500, start=0):
        for i in range(start, self.maxIDep+1):
            self.focus_idep(i)
            RunForceAtlas2_nooverlap(iters)
            self.move_right_auto(i)
        self.setAllVisible()

    def auto_relayout_all(self, iters=500, start=0):
        for i in range(start, self.maxIDep+1):
            self.visible_idep(i)
            RunForceAtlas2_nooverlap(iters)
            self.move_right_auto(i)
        self.setAllVisible()

    def setVisible(self, subg):
        self.globals['visible'] = subg

    def setAllVisible(self):
        self.setVisible(self.g)

def setVisible(gs, subgraph):
    gs['visible'] = subgraph

"""
Util Functions
"""
"""
@type arr string
@param arr has the format "arrname[arrsize]"
"""
def getarrSize(arr):
    return int(arr[arr.find('[')+1:-1])
def getarrName(arr):
    return arr.split('[')[0]
"""
main
"""
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=
      "HASE cosntraint graph analysis cli mode (python3)")
    parser.add_argument("--ignore-evaluation", action="store_true",
            help="Ignore the evaluation list in the query and always perform "
                 "full graph analysis")
    parser.add_argument("--evalinst", action="store", type=str, default=None,
            help="a list of kinst id to evaluate, separated by comma")
    parser.add_argument("--evalnid", action="store", type=str, default=None,
            help="a list of node id to evaluate, separated by comma")
    parser.add_argument("--recordUN", action="store", type=str, default=None,
            help="a list of array name, whose update lists should be "
                 "concretized, separated by comma")
    parser.add_argument("--indep-thres", action="store", type=int, default=0,
            help="The minimum indirect depth of an index value used to access "
                 "recordUN (default is 0)")
    parser.add_argument("--datarec-out", action="store", type=str,
            default="", help="A file to output datarec.cfg")
    parser.add_argument("--UN-constraints", type=str, default=None,
            help="A file each line giving a path of a (simplified) "
                 "constraint graph json file.")
    parser.add_argument("--recordUNCFG", type=str, default=None,
            help="A datarec.cfg, containing all selected kinsts")
    parser.add_argument("--getUN", action="store_true",
            help="Analyze all UN and print the length of each UN")
    parser.add_argument("--noptwrite", action="store_true",
            help="Do not assume the minimum data entry to record is 8B")
    parser.add_argument("graph_json", type=str, action="store",
            help="the json file describing the cosntraint graph")
    parser.add_argument("selected_kinst", nargs='*', type=str,
            help="kinst already chosen to be recorded")
    args = parser.parse_args()
    PyGraph.PTWRITE = not args.noptwrite
    if args.UN_constraints is not None and args.recordUNCFG is not None:
        # Require recursive optimization. disable idep calculation
        RecordableInst.SUBGRAPH = False
        # recursively optimize all kinst selected by previous iterations
        PreOptimizedUNKinstset = set(open(args.recordUNCFG).read().splitlines())
        UN_constraints = open(args.UN_constraints).read().splitlines()
        changed = True
        OptimizedUNKinstset = set()
        while changed:
            changed = False
            for graph_path in UN_constraints:
                print("Optimizing on %s" % (graph_path))
                constraint_graph = json.load(open(graph_path))
                PyG = PyGraph.buildFromPyDict(constraint_graph)
                new_recinsts, new_PyG = PyG.recursiveOptimizeRecKInstL(\
                        PreOptimizedUNKinstset)
                for recinst in new_recinsts:
                    OptimizedUNKinstset.add(recinst.kinst)
            if OptimizedUNKinstset != PreOptimizedUNKinstset:
                print("Optimized from: %s" %
                        ','.join(PreOptimizedUNKinstset))
                print("To :%s" % ','.join(OptimizedUNKinstset))
                PreOptimizedUNKinstset = OptimizedUNKinstset
                OptimizedUNKinstset = set()
                Changed = True
        kinst_sorted = sorted(list(OptimizedUNKinstset))
        print("OptimizedUNKinstset: %s" % ','.join(kinst_sorted))
        if len(args.datarec_out) > 0:
            f = open(args.datarec_out, "w")
            f.write("%s\n" % '\n'.join(kinst_sorted))
            f.close()
        sys.exit(0)
    graph = json.load(open(args.graph_json))
    h = PyGraph.buildFromPyDict(graph)
    print("%d nodes, %d edges, max idep %d" % (len(h.gynodes),
        len(h.gyedges), h.max_idep()))
    query_nodes = set()
    array_to_concretize = set()
    if not args.ignore_evaluation:
        query_nodes |= set(filter(lambda n: n.category == "Q", h.gynodes))
    if args.evalinst is not None:
        evalinst = set(args.evalinst.split(','))
        query_nodes |= set(filter(lambda n: n.kinst in evalinst, h.gynodes))
    if args.evalnid is not None:
        evalnid = set(args.evalnid.split(','))
        query_nodes |= set(filter(lambda n: n.id in evalnid, h.gynodes))
    if args.recordUN is not None:
        array_to_concretize |= \
            set([s for s in args.recordUN.split(',') if len(s) > 0])

    input_kinst_list, subh = h.buildRecKInstL(args.selected_kinst)

    if len(input_kinst_list) > 0:
        print("Assuming record:")
        print(h.getRecInstsInfo(input_kinst_list))

    if args.getUN:
        # @type: Dict(arrayname -> list of Indirect UN len)
        arr2UNlenID = {}
        # @type: Dict(arrayname -> list of Direct UN len)
        arr2UNlenD = {}
        # @type: Dict(arrayname -> list of [Indirect Read CNT, Direct Read CNT])
        arr2ReadRef = {}
        # @type: Dict(arrayname -> list of indirect depth of read/write
        # accessing this array
        # NOTE: this is currently not very helpful and not used in ranking
        arr2Indep = {}
        # a heuristic score for each array used for ranking
        # Current heuristic:
        #   score = sum of [(accumulated length of a UN Node * indirect depth of
        #   indirect read using a UN node as index) for all UN nodes in a chain]
        # @type: Dict(arrayname -> integer)
        arr2score = {}
        for n in subh.all_nodes_topo_order:
            if str(n.kind) == "UN":
                arr2UNlenD.setdefault(n.root, [0])
                arr2UNlenID.setdefault(n.root, [0])
                if any([e.weight == 1.5  for e in subh.edges[n.id]]):
                    arr2UNlenID[n.root][-1] += 1
                else:
                    arr2UNlenD[n.root][-1] += 1
                readNodes = [e.source for e in subh.redges[n.id] \
                        if str(e.source.kind) == "3"]
                if len(readNodes) > 0:
                    # this UpdateNode is pointed to by some ReadExpr
                    # a new chain of UN appears
                    IndirectReads = list(filter(lambda n: any([\
                            e.weight == 1.5 and str(e.target.kind) != "0" \
                            for e in subh.edges[n.id]]),
                            readNodes))
                    arr2ReadRef.setdefault(n.root, []).append(
                        [len(IndirectReads),
                         len(readNodes)-len(IndirectReads)])
                    arr2Indep.setdefault(n.root, []).extend([
                        subh.idep_map[n.id] for n in readNodes
                        ])
                    score = 0
                    # accumulated length of UN chain at this point
                    accu_l = sum(arr2UNlenID[n.root])
                    score = sum([accu_l*(subh.idep_map[n.id]+1) for n in
                        IndirectReads])
                    arr2score[n.root] = arr2score.get(n.root, 0) + score
                    arr2UNlenID[n.root].append(0)
                    arr2UNlenD[n.root].append(0)
            if str(n.kind) == "3" and (n.id in subh.edges) and \
               all([str(e.target.kind) != "UN" for e in subh.edges[n.id]]) and \
               any([e.weight == 1.5 for e in subh.edges[n.id]]):
                # this is a Read Node not depending on any UN
                # Should keep tracking of its root
                arr2UNlenID.setdefault(n.root, [0])
                arr2UNlenD.setdefault(n.root, [0])
                arr2ReadRef.setdefault(n.root, [[0,0]])[-1][0] += 1
                arr2Indep.setdefault(n.root, []).append(subh.idep_map[n.id])
        # rank array name
        # (heuristic_score, length, arr_size)
        ranked_arr = sorted(arr2UNlenD.keys(),
                key=lambda arr:
                  (arr2score.get(arr,0),
                   sum(arr2UNlenID.get(arr, 0),
                   getarrSize(arr))),
                reverse=True)
        size_ranked_arr = sorted(arr2UNlenD.keys(),
                key=lambda arr: getarrSize(arr),
                reverse=True)
        print("# Format:\n"
              "# Array: name[size]\n"
              "#    (indirect UNlen, direct)(indirect ref, direct ref)" )
        for arr in ranked_arr:
            UNLID = arr2UNlenID[arr]
            UNLD = arr2UNlenD[arr]
            print("Array: %s" % arr)
            accu_l = 0
            accumulate_len = []
            for l_ID, l_D in zip(UNLID, UNLD):
                accu_l += l_ID + l_D
                accumulate_len.append(accu_l)
            print("\t%s" % ', '.join(["(%d,%d)(%d,%d)@[%d]" % \
                    (l_ID, l_D, IR, R, accu_l) \
                for l_ID, l_D, (IR, R) , accu_l in \
                zip(UNLID, UNLD, arr2ReadRef.get(arr, [[0,0]]), accumulate_len)]))
            indep_l = arr2Indep.get(arr, [0])
            print("\tindep: avg %f, max %d, score %d" %
                    (sum(indep_l)/len(indep_l),
                        max(indep_l), arr2score.get(arr,0)))
        target_array=set()
        target_array.add(ranked_arr[0])
        target_array.add(size_ranked_arr[0])
        print("target_array:%s" %
                (','.join([getarrName(arr) for arr in target_array])))
    elif len(query_nodes) > 0:
        for n in query_nodes:
            print("query %s, kinst: %s" % (n.label, n.kinst))
        initial_strategy = set([x.id for x in query_nodes])
        new_strategy = subh.recursiveOptimizeRecNids(initial_strategy)
        kinsts = subh.GetKInstSetFromNids(new_strategy)
        recinsts, graph_withrecinsts = subh.buildRecKInstL(kinsts)
        kinst_sorted = sorted(kinsts)
        if len(recinsts) > 0:
            print("To concretize %d nodes" % len(query_nodes))
            print(subh.getRecInstsInfo(recinsts))
            print("Python kinst list:")
            print("\"%s\"" % '\",\"'.join(kinst_sorted))
            print("Datarec.cfg:")
            datarecCFG= "%s\n" % '\n'.join(kinst_sorted)
            print(datarecCFG)
            if len(args.datarec_out) > 0:
                f = open(args.datarec_out, "w")
                f.write(datarecCFG)
                f.close()
            print("All Label:")
            print("%s" % ', '.join(sorted([subh.id_map[nid].label
                for kinst in kinst_sorted for nid in subh.kinst2nodes[kinst]])))
        else:
            print("Already Concretized!")
    elif args.recordUN is not None:
        if len(array_to_concretize) == 0:
            print("Empty array list")
            sys.exit(0)
        # Require recursive optimization. disable idep calculation
        RecordableInst.SUBGRAPH = False
        recnids = subh.UpdateListConcretize(array_to_concretize,
                args.indep_thres)
        kinsts = subh.GetKInstSetFromNids(recnids)
        recinsts, graph_withrecinsts = subh.buildRecKInstL(kinsts)
        kinst_sorted = sorted(kinsts)
        print("To concretize UN upon %s" % ','.join(array_to_concretize))
        print(subh.getRecInstsInfo(recinsts))
        print("Python kinst list:")
        print("\"%s\"" % '\",\"'.join(kinst_sorted))
        print("Datarec.cfg:")
        datarecCFG= "%s\n" % '\n'.join(kinst_sorted)
        print(datarecCFG)
        if len(args.datarec_out) > 0:
            f = open(args.datarec_out, "w")
            f.write(datarecCFG)
            f.close()
        print("All Label:")
        print("%s" % ', '.join(sorted([subh.id_map[nid].label
            for kinst in kinst_sorted for nid in subh.kinst2nodes[kinst]])))
    else:
        r = subh.analyze_recordable(input_kinst_list)
        print("%d recordable instructions" % len(r))

        print("Heuristic: Coverage Score Highest 10:")
        sr = subh.sortRecInstsbyCoverageScore(r)
        subh.printCandidateRecInstsInfo(sr[-10:])

        print("Heuristic: Coverage Freq Score Highest 10:")
        srf = subh.sortRecInstsbyCoverageScoreFreq(r)
        subh.printCandidateRecInstsInfo(srf[-10:])

        print("Heuristic: Node Reduction Highest 10:")
        nreduction = subh.sortRecInstsbyNodeReduction(r)
        subh.printCandidateRecInstsInfo(nreduction[-10:])

        print("Heuristic: Node Reduction Per Byte Highest 10:")
        nreduction_B = subh.sortRecInstsbyNodeReductionPerByte(r)
        subh.printCandidateRecInstsInfo(nreduction_B[-10:])

        print("Heuristic: Remain Score and RecordSize High (worse) to Low (better)")
        rsf = subh.sortRecInstbyRemainScoreFreq(r)
        subh.printCandidateRecInstsInfo(reversed(rsf))
