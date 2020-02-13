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
import org.openide.util.Lookup as Lookup
import org.gephi.layout.api.LayoutController
LayoutController = Lookup.getDefault().lookup(org.gephi.layout.api.LayoutController)
import org.gephi.visualization.VizController
VizController = Lookup.getDefault().lookup(org.gephi.visualization.VizController)
import org.gephi.layout.plugin.forceAtlas2.ForceAtlas2Builder as ForceAtlas2
import java.awt.Color as jcolor
import time
import org.gephi.scripting.wrappers.GyNode as GyNode

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

@type kinst: str
@param kinst: a global unique identifier of this instruction

@type width:int
@param width: the width of the result(destination register) of this instruction

@type freq: int
@param freq: how many times this instruction got executed in the entire trace

@type rec_nodes: List(node.id)
@param rec_nodes: what symbolic nodes are directly recorded if you record this instruction

@type hidden_nodes: List(node.id)
@param hidden_nodes: what symbolic nodes related to other instruction but still can be
concretized if you record this instruction

@type concretized_nodes: set(node.id)
@param concretized_nodes: what symbolic nodes (not ConstantExpr) can be
concretized after you record this instruction. Note that this list will include
rec_nodes and hidden_nodes, but will not include any ConstantExpr nodes.

A RecordableInstruction will be represented by a list of RecordableNode
"""
class RecordableInst(object):
    def __init__(self, kinst, width, freq, rec_nodes, hidden_nodes, concretized_nodes):
        self.kinst = kinst
        self.width = width
        self.freq = freq
        self.rec_nodes = rec_nodes
        self.hidden_nodes = hidden_nodes
        self.concretized_nodes = concretized_nodes
        # sanity check
        if len(set(rec_nodes)) != len(rec_nodes):
            raise RuntimeError("duplicate nodes in rec_nodes")
        if len(set(hidden_nodes)) != len(hidden_nodes):
            raise RuntimeError("duplicate nodes in hidden_nodes")
        if len(set(concretized_nodes)) != len(concretized_nodes):
            raise RuntimeError("duplicate nodes in concretized_nodes")
        if width == 0:
            raise RuntimeError("Zero Width instruction")

    def __str__(self):
        return "kinst: %s, freq: %d, %d nodes recorded, %d nodes hidden,"\
                "%d nodes concretized" % (self.kinst, self.freq,\
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
    graph = None
    # edge map node.id -> list of edges starting from node
    edges = {}
    # reverse edge map, node.id -> list of edges ending in node
    redges = {}
    # map node.id -> node
    id_map = {}
    # node.id -> topological id (0..|V|-1)
    # note that the klee expression graph looks like
    # [operator] -> [operand0]
    #            -> [operand1]
    # so the edge represents [dependant] -> [dependency]
    # The assigned topological id: [dependant] > [dependency]
    # aka [result] > [operands]
    topological_map = None
    # innodes: no in edges, outnodes: no out edges
    # content is node
    innodes = None
    outnodes= None
    # @type: Dict(str->List(GyNode))
    kinst2nodes = None

    def __init__(self, graph):
        self.graph = graph
        self.innodes = set()
        self.outnodes = set()
        for e in graph.edges:
            self.edges.setdefault(e.source.id, []).append(e)
            self.redges.setdefault(e.target.id, []).append(e)
        for n in graph.nodes:
            self.id_map[n.id] = n
            if n.indegree == 0:
                self.innodes.add(n)
            if n.outdegree == 0:
                self.outnodes.add(n)
        self.topological_sort()
        self.build_kinst2nodes()

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
        for node in self.graph.nodes:
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
    Dependency: topological_map
    build the mapping from recordable instructions to all associated nodes in
    the constraint graph
    """
    def build_kinst2nodes(self):
        self.kinst2nodes = {}
        all_nodes_topo_order = sorted(self.graph.nodes,
                key=lambda n: self.topological_map[n.id])
        for node in all_nodes_topo_order:
            if isKInstValid(node):
                self.kinst2nodes.setdefault(node.kinst, []).append(node.id)


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

    @rtype: List(RecordableInst)
    @return: a list all beneficial recordable instructions
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
        for recinst in recinsts:
            for nid in recinst.rec_nodes:
                concretized_set.add(nid)
        # @type: List(RecordableInst)
        result = []
        all_nodes_topo_order = sorted(self.graph.nodes,
                key=lambda n: self.topological_map[n.id])
        # Pre Process
        for node in all_nodes_topo_order:
            # find the closure of given concretized_set
            if node.id in self.edges and \
               all([(e.target.kind == "0") or (e.target.id in concretized_set) \
                    for e in self.edges[node.id]]):
                concretized_set.add(node)

        for seqid, n in enumerate(all_nodes_topo_order):
            if (isKInstValid(n)) and (n.id not in checked_kinst_set):
                local_concretized_set = concretized_set.copy()
                hidden_nodes = []
                for nid in self.kinst2nodes[n.kinst]:
                    checked_kinst_set.add(nid)
                    local_concretized_set.add(nid)
                for node in all_nodes_topo_order[seqid+1:]:
                    # skip ConstantExpr and nodes without out edges
                    # only consider nontrivial intermediate nodes
                    if (node.kind != "0") and (node.id in self.edges) and \
                    (node.id not in local_concretized_set):
                        const_nodes = [e.target.id for e in self.edges[node.id]
                                if e.target.kind == "0"]
                        known_symbolic_nodes = [e.target.id for e in
                                self.edges[node.id] if e.target.id in
                                local_concretized_set]
                        if len(const_nodes) + len(known_symbolic_nodes) == \
                        len(self.edges[node.id]):
                            local_concretized_set.add(node.id)
                        if len(const_nodes) + len(known_symbolic_nodes) > \
                        self.edges[node.id]:
                            raise RuntimeError("sum of out edges wrong")
                        if len(known_symbolic_nodes) > 0 and isKInstValid(node):
                            checked_kinst_set.add(node.id)
                            hidden_nodes.append(node.id)
                result.append(RecordableInst(n.kinst, int(n.Width), int(n.Freq),
                    self.kinst2nodes[n.kinst], hidden_nodes,
                    local_concretized_set - concretized_set))
        return result

    """
    Concretize the graph given nodes in `nodes_id_set` are already concretized
    Mark all nodes can be concretized with a special color (like white)
    """
    def concretize_set(self, nodes_id_set):
        # set of node.id, which can be concretized
        concretized_set = set(nodes_id_set)
        all_nodes_topo_order = sorted(self.graph.nodes,
                key=lambda n: self.topological_map[n.id])
        for n in all_nodes_topo_order:
            if n.kind == "0":
                concretized_set.add(n.id)
            # only node has out edges can be concretized
            elif n.id in self.edges:
                concretizable = all([e.target.id in concretized_set
                    for e in self.edges[n.id]])
                if concretizable:
                    concretized_set.add(n.id)
        return concretized_set

    """
    @rtype: float
    @return: the heuristic score of this Recordable Instruction. expression
    width and indirect depth are considered
    """
    def coverageScore(self, recinst):
        return float(sum([int(self.id_map[nid].Width)*int(self.id_map[nid].idep)\
                for nid in recinst.concretized_nodes]))
    """
    @type recinsts: List(RecordableInst)
    @param recinsts: list of recordable instructions from analyze_recordable()

    @rtype: List(RecordableInst)
    @return: sorted list, sorted according to coverageScore
    """
    def sortRecInstsbyCoverageScore(self, recinsts):
        return sorted(recinsts, key=lambda n: self.coverageScore(n))

    """
    @rtype: float
    @return: Heuristics take instruction frequency into consideration
    """
    def coverageScoreFreq(self, recinst):
        if recinst.freq == 0:
            return 0
        else:
            return self.coverageScore(recinst) / (recinst.freq * recinst.width)

    """
    same as above
    """
    def sortRecInstsbyCoverageScoreFreq(self, recinsts):
        return sorted(recinsts, key=lambda n: self.coverageScoreFreq(n))

    """
    @rtype: str
    @return: A string represents important information of recorded nodes in a
    RecordableInst
    """
    def getRecNodesInfo(self, recinst):
        return ', '.join([self.id_map[nid].label for nid in recinst.rec_nodes])

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

    def MarkNodesWhiteByID(self, nodes_id):
        self.ColorNodesByID(nodes_id, jcolor.white)

    def MarkNodesRedByID(self, nodes_id):
        self.ColorNodesByID(nodes_id, jcolor.red)

    """
    Visualize a RecordableInst on the graph
    @type recinst: RecordableInst
    @param recinst: the inst to show
    """
    def VisualizeRecordableInst(self, recinst):
        non_hidden_nodes = recinst.concretized_nodes - set(recinst.rec_nodes) - \
            set(recinst.hidden_nodes)
        self.MarkNodesRedByID(recinst.rec_nodes)
        self.MarkNodesWhiteByID(non_hidden_nodes)
        self.ColorNodesByID(recinst.hidden_nodes, jcolor.green)


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
                v.size = 15
        # nodes pointed by cross-layer edges but no intra-layer edges are roots
        # nodes pointed by cross-layer edges will be enlarged
        for e in self.g.edges:
            if e.weight==1.5:
                if e.target.idepi == e.source.idepi + 1:
                    self.all_root_nodes.add(e.target.id)
                    e.target.size = 15
                self.idep_index_score[e.target.id] = self.idep_index_score.get(e.target.id, 0) + e.source.idepi
        for e in self.g.edges:
            if (e.target.idepi == e.source.idepi) and \
                    (e.target.id in self.all_root_nodes):
                        self.all_root_nodes.remove(e.target.id)
                        del self.idep_index_score[e.target.id]
        self.idep_roots = []

        """
        0 level, root nodes have special definition on this level:
          1. root nodes are nodes without indegree
          2. root nodes are sorted based on their outdegree
        """
        self.idep_roots.append(
                sorted(
                    [v for v in self.idep_subg[0].nodes if v.indegree == 0],
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
    """
    Layout the root nodes of a given level
    Will evenly spread roots (sorted) along Y-axis on the left most border
    (min x-axis) of that level
    """
    def layout_idep_root(self, idep):
        subg = self.idep_subg[idep]
        roots = self.idep_roots[idep]
        layout_ymin = min([v.y for v in subg.nodes])
        layout_ymax = max([v.y for v in subg.nodes])
        layout_xmin = min([v.x for v in subg.nodes])
        ystep = (layout_ymax - layout_ymin) / (len(roots)+1)
        for (i,r) in enumerate(roots):
            r.x = layout_xmin
            r.y = layout_ymin + (i+1) * ystep

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

    def auto_layout_all(self, iters=500):
        for i in range(self.maxIDep+1):
            self.focus_idep(i)
            RunForceAtlas2_nooverlap(iters)
            self.move_right_auto(i)
        self.setAllVisible()

    def auto_relayout_all(self, iters=500):
        for i in range(self.maxIDep+1):
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
