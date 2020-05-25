import json
import argparse
import random

from hase import PyGraph

def read_nodes(graph_json):
    graph = json.load(open(graph_json))
    h = PyGraph.buildFromPyDict(graph)
    return list(h.gynodes)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("target", type=int, action="store",
                      help="target")
    parser.add_argument("graph_json", action="store", type=str,
                      help="the json file describing the constraint graph")
    parser.add_argument("cfg", action="store", type=str,
                      help="destination of configuration file")
    args = parser.parse_args()

    nodes = read_nodes(args.graph_json)

    nodes = sorted(nodes, key=lambda node: node.freq * node.width)
    target = args.target

    i = 0
    for n in nodes:
        if n.freq * n.width < target:
            i += 1

    selected = {}
    cnt = 0
    j = 0
    while cnt < target or j > 5000:
        j += 1
        r = random.randint(0, i+1)
        n = nodes[r]
        if n.id in selected:
            continue
        if n.ispointer == True:
            continue
        if n.kinst == "N/A":
            continue
        selected[n.id] = n.kinst
        cnt += n.freq * n.width

    print("total: {}".format(cnt))

    for key in selected:
        print(selected[key])

    with open(args.cfg, 'w') as f:
        for key in selected:
            f.write("{}\n".format(selected[key]))


        


