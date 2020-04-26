"""
Used to convert dict type node(loaded from json) to a
"org.gephi.scripting.wrappers.GyNode" compatibale class.
"""
class FakeGyNode(object):
    def __init__(self, nodeid, propdict):
        self.__setattr__('id', nodeid)
        for key, value in propdict.items():
            self.__setattr__(key.lower(), value)

class FakeGyEdge(object):
    # source and target should be reference to FakeGyNode
    def __init__(self, source, target, propdict):
        self.source = source
        self.target = target
        for key, value in propdict.items():
            if key != "source" and key != "target":
                self.__setattr__(key.lower(), value)
