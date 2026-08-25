from collections import namedtuple
from collections import deque

Action = namedtuple("Action", ['description', 'callable'])


class ActionsList(object):
    """ Collection of actions that doesn't allow duplicate values
        and preserves the order in which elements are added to it.
    """
    def __init__(self, actions=None):
        self.items = dict()
        self.data = deque()

        if actions is not None:
            self.extend(actions)

    def append(self, action):
        """ Only appends action if there isn't one with the same description
            already in the collection """
        if action.description not in self.items:
            self.items[action.description] = action.callable
            self.data.append(action)

    def extend(self, actions):
        for action in actions:
            self.append(action)

    def insert(self, i, action):
        if action.description not in self.items:
            self.items[action.description] = action.callable
            self.data.insert(i, action)

    def clear(self):
        self.data.clear()
        self.items.clear()

    def remove(self, action):
        if action.description in self.items:
            del self.items[action.description]
            self.data.remove(action)

    def pop(self):
        action = self.data.pop()
        del self.items[action.description]

    def popleft(self):
        action = self.data.popleft()
        del self.items[action.description]

    def __delitem__(self, index):
        action = self.data[index]
        del self.items[action.description]
        del self.data[index]

    def __len__(self):
        return len(self.data)

    def __getitem__(self, i):
        return self.data[i]

    def __iter__(self):
        return self.data.__iter__()
