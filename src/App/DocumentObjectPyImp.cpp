/***************************************************************************
 *   Copyright (c) Jürgen Riegel          (juergen.riegel@web.de) 2007     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#include "DocumentObject.h"
#include "Document.h"
#include "Expression.h"

// inclusion of the generated files (generated out of DocumentObjectPy.xml)
#include <App/DocumentObjectPy.h>
#include <App/DocumentObjectPy.cpp>

using namespace App;

// returns a string which represent the object e.g. when printed in python
std::string DocumentObjectPy::representation(void) const
{
    return std::string("<Document object>");
}

Py::String DocumentObjectPy::getName(void) const
{
    DocumentObject* object = this->getDocumentObjectPtr();
    const char* internal = object->getNameInDocument();
    if (!internal) {
        throw Py::RuntimeError(std::string("This object is currently not part of a document"));
    }
    return Py::String(std::string(internal));
}

Py::Object DocumentObjectPy::getDocument(void) const
{
    DocumentObject* object = this->getDocumentObjectPtr();
    Document* doc = object->getDocument();
    if (!doc) {
        return Py::None();
    }
    else {
        return Py::Object(doc->getPyObject(), true);
    }
}

PyObject*  DocumentObjectPy::addProperty(PyObject *args)
{
    char *sType,*sName=0,*sGroup=0,*sDoc=0;
    short attr=0;
    std::string sDocStr;
    PyObject *ro = Py_False, *hd = Py_False;
    if (!PyArg_ParseTuple(args, "s|ssethO!O!", &sType,&sName,&sGroup,"utf-8",&sDoc,&attr,
        &PyBool_Type, &ro, &PyBool_Type, &hd))     // convert args: Python->C
        return NULL;                             // NULL triggers exception

    if (sDoc) {
        sDocStr = sDoc;
        PyMem_Free(sDoc);
    }

    App::Property* prop=0;
    try {
        prop = getDocumentObjectPtr()->addDynamicProperty(sType,sName,sGroup,sDocStr.c_str(),attr,
            PyObject_IsTrue(ro) ? true : false, PyObject_IsTrue(hd) ? true : false);
    }
    catch (const Base::Exception& e) {
        throw Py::RuntimeError(e.what());
    }
    if (!prop) {
        std::stringstream str;
        str << "No property found of type '" << sType << "'" << std::ends;
        throw Py::Exception(Base::BaseExceptionFreeCADError,str.str());
    }

    return Py::new_reference_to(this);
}

PyObject*  DocumentObjectPy::removeProperty(PyObject *args)
{
    char *sName;
    if (!PyArg_ParseTuple(args, "s", &sName))
        return NULL;

    try {
        bool ok = getDocumentObjectPtr()->removeDynamicProperty(sName);
        return Py_BuildValue("O", (ok ? Py_True : Py_False));
    }
    catch (const Base::Exception& e) {
        throw Py::RuntimeError(e.what());
    }
}

PyObject*  DocumentObjectPy::supportedProperties(PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))     // convert args: Python->C
        return NULL;                    // NULL triggers exception

    std::vector<Base::Type> ary;
    Base::Type::getAllDerivedFrom(App::Property::getClassTypeId(), ary);
    Py::List res;
    for (std::vector<Base::Type>::iterator it = ary.begin(); it != ary.end(); ++it) {
        Base::BaseClass *data = static_cast<Base::BaseClass*>(it->createInstance());
        if (data) {
            delete data;
            res.append(Py::String(it->getName()));
        }
    }
    return Py::new_reference_to(res);
}

PyObject*  DocumentObjectPy::touch(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))     // convert args: Python->C 
        return NULL;                    // NULL triggers exception 
    getDocumentObjectPtr()->touch();
    Py_Return;
}

PyObject*  DocumentObjectPy::purgeTouched(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))     // convert args: Python->C 
        return NULL;                    // NULL triggers exception 
    getDocumentObjectPtr()->purgeTouched();
    Py_Return;
}

Py::List DocumentObjectPy::getState(void) const
{
    DocumentObject* object = this->getDocumentObjectPtr();
    Py::List list;
    bool uptodate = true;
    if (object->isTouched()) {
        uptodate = false;
        list.append(Py::String("Touched"));
    }
    if (object->isError()) {
        uptodate = false;
        list.append(Py::String("Invalid"));
    }
    if (object->isRecomputing()) {
        uptodate = false;
        list.append(Py::String("Recompute"));
    }
    if (object->isRestoring()) {
        uptodate = false;
        list.append(Py::String("Restore"));
    }
    if (uptodate) {
        list.append(Py::String("Up-to-date"));
    }
    return list;
}

Py::Object DocumentObjectPy::getViewObject(void) const
{
    try {
        Py::Module module(PyImport_ImportModule("FreeCADGui"),true);
        if (!module.hasAttr("getDocument")) {
            // in v0.14+, the GUI module can be loaded in console mode (but doesn't have all its document methods)
            return Py::None();
        }
        Py::Callable method(module.getAttr("getDocument"));
        Py::Tuple arg(1);
        arg.setItem(0, Py::String(getDocumentObjectPtr()->getDocument()->getName()));
        Py::Object doc = method.apply(arg);
        method = doc.getAttr("getObject");
        arg.setItem(0, Py::String(getDocumentObjectPtr()->getNameInDocument()));
        Py::Object obj = method.apply(arg);
        return obj;
    }
    catch (Py::Exception& e) {
        if (PyErr_ExceptionMatches(PyExc_ImportError)) {
            // the GUI is not up, hence None is returned
            e.clear();
            return Py::None();
        }
        // FreeCADGui is loaded, so there must be wrong something else
        throw; // re-throw
    }
}

Py::List DocumentObjectPy::getInList(void) const
{
    Py::List ret;
    std::vector<DocumentObject*> list = getDocumentObjectPtr()->getInList();

    for (std::vector<DocumentObject*>::iterator It=list.begin();It!=list.end();++It)
        ret.append(Py::Object((*It)->getPyObject(), true));

    return ret;
}

Py::List DocumentObjectPy::getOutList(void) const
{
    Py::List ret;
    std::vector<DocumentObject*> list = getDocumentObjectPtr()->getOutList();

    for (std::vector<DocumentObject*>::iterator It=list.begin();It!=list.end();++It)
        ret.append(Py::Object((*It)->getPyObject(), true));

    return ret;
}

PyObject*  DocumentObjectPy::setExpression(PyObject * args)
{
    char * path = NULL;
    PyObject * expr;
    char * comment = 0;

    if (!PyArg_ParseTuple(args, "sO|s", &path, &expr, &comment))     // convert args: Python->C
        return NULL;                    // NULL triggers exception

    App::ObjectIdentifier p(ObjectIdentifier::parse(getDocumentObjectPtr(), path));

    if (Py::Object(expr).isNone())
        getDocumentObjectPtr()->setExpression(p, boost::shared_ptr<Expression>());
    else if (PyString_Check(expr)) {
        const char * exprStr = PyString_AsString(expr);
        boost::shared_ptr<Expression> shared_expr(ExpressionParser::parse(getDocumentObjectPtr(), exprStr));

        getDocumentObjectPtr()->setExpression(p, shared_expr, comment);
    }
    else if (PyUnicode_Check(expr)) {
        PyObject* unicode = PyUnicode_AsEncodedString(expr, "utf-8", 0);
        if (unicode) {
            std::string exprStr = PyString_AsString(unicode);
            Py_DECREF(unicode);
            boost::shared_ptr<Expression> shared_expr(ExpressionParser::parse(getDocumentObjectPtr(), exprStr.c_str()));

            getDocumentObjectPtr()->setExpression(p, shared_expr, comment);
        }
        else {
            // utf-8 encoding failed
            return 0;
        }
    }
    else
        throw Py::TypeError("String or None expected.");
    Py_Return;
}


PyObject *DocumentObjectPy::getCustomAttributes(const char* /*attr*/) const
{
    return 0;
}

int DocumentObjectPy::setCustomAttributes(const char* attr, PyObject *obj)
{
    // search in PropertyList
    Property *prop = getDocumentObjectPtr()->getPropertyByName(attr);
    if (prop) {
        // Read-only attributes must not be set over its Python interface
        short Type =  getDocumentObjectPtr()->getPropertyType(prop);
        if (Type & Prop_ReadOnly) {
            std::stringstream s;
            s << "'DocumentObject' attribute '" << attr << "' is read-only"; 
            throw Py::AttributeError(s.str());
        }

        try {
            prop->setPyObject(obj);
        }
        catch (const Base::TypeError& e) {
            std::stringstream s;
            s << "Property '" << prop->getName() << "': " << e.what();
            throw Py::TypeError(s.str());
        }
        return 1;
    } 

    return 0;
}
