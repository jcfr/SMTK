# =============================================================================
#
#  Copyright (c) Kitware, Inc.
#  All rights reserved.
#  See LICENSE.txt for details.
#
#  This software is distributed WITHOUT ANY WARRANTY; without even
#  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#  PURPOSE.  See the above copyright notice for more information.
#
# =============================================================================

from ._smtkPybindOperation import *

"""
Operation parameters are simply instances of smtk.attribute.Attribute. We wrap
smtk.operation.Operation.parameters() in a python <_create_api> that constructs
a more user-friendly API to the python interface for operation parameters.
"""


def _create_api(parameters):

    def bind(instance, name, func):
        """
        Bind the function *func* to *instance*  with provided name *name*. The
        provided *func* should accept the instance as the first argument, i.e.
        "self".
        """
        bound_method = func.__get__(instance, instance.__class__)
        setattr(instance, name, bound_method)
        return bound_method

    def api_for_item(parameters, item):
        name = item.name()
        import re
        nameList = re.split(r"[^a-zA-Z0-9]", item.name().title())
        if hasattr(item, 'value'):
            set_attr = 'set' + ''.join(nameList)
            get_attr = nameList[0].lower()
            if len(nameList) > 1:
                get_attr += ''.join(nameList[1:])

            def setValue(self, *argv):
                index = 0
                if len(argv) > 1:
                    index = argv[0]
                return self.find(name).setValue(index, argv[-1])
            bind(parameters, set_attr, setValue)

            def value(self, index=0):
                return self.find(name).value(index)
            bind(parameters, get_attr, value)
        if item.isOptional():
            enable_attr = 'enable' + ''.join(nameList)
            enabled_attr = nameList[0].lower()
            if len(nameList) > 1:
                enabled_attr += ''.join(nameList[1:])
            enabled_attr += 'Enabled'

            def enable(self, choice):
                return self.find(name).setIsEnabled(choice)
            bind(parameters, enable_attr, enable)

            def isEnabled(self):
                return self.find(name).isEnabled()
            bind(parameters, enabled_attr, isEnabled)

        if hasattr(item, 'isDiscrete'):
            setindex_attr = 'set' + ''.join(nameList) + 'Index'

            def setDiscreteIndex(self, index, value):
                return self.find(name).setDiscreteIndex(index, value)
            bind(parameters, setindex_attr, setDiscreteIndex)
            getindex_attr = nameList[0].lower()
            if len(nameList) > 1:
                getindex_attr += ''.join(nameList[1:])
            getindex_attr += "Index"

            def discreteIndex(self, index):
                return self.find(name).discreteIndex(index)
            bind(parameters, getindex_attr, discreteIndex)

    for i in range(parameters.numberOfItems()):
        api_for_item(parameters, parameters.item(i))


def _params(self):
    params = self._parameters()
    _create_api(params)
    return params


setattr(Operation, 'parameters', _params)

del _params

""" Provide a method to register all operations in a module.
"""


def _registerModuleOperations(self, module):
    """Register all SMTK operations in a python module to this manager.

    Note this does not recurse modules; only operations directly inside
    the module are imported.
    """
    for item in dir(module):
        try:
            thing = getattr(module, item)
            if issubclass(thing, Operation):
                self.registerOperation(module.__name__, item)
        except:
            continue


setattr(Manager, 'registerModuleOperations', _registerModuleOperations)
del _registerModuleOperations
