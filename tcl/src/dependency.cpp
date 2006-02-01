/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : dependency.cpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM problem for TCL
 
This file implements an object type for TCL that contains an RPM dependency report

*****************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "rpm_for_tcl.hpp"
#include "dependency.hpp"
#include <rpm/rpmds.h>

// C-style "vftable" for this class - gosh, it would be soo much easier were TCL
// written in C++....

RPMDependency_Obj::MyType RPMDependency_Obj::mytype;

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::MyType::MyType                    *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Register type with TCL                               *
*************************************************************************/
RPMDependency_Obj::MyType::MyType(void)
{
   name = "RPMDependency";
   freeIntRepProc   = RPMDependency_Obj::FreeInternalRep;
   dupIntRepProc    = RPMDependency_Obj::DupInternalRep;
   updateStringProc = RPMDependency_Obj::UpdateString;
   setFromAnyProc   = RPMDependency_Obj::SetFromAny;
   Tcl_RegisterObjType(this);
}
/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::MyType::~MyType                   *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   deregister type with TCL                             *
*************************************************************************/
RPMDependency_Obj::MyType::~MyType()
{
}

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::FreeInternalRep                   *
* ARGUMENTS     :   this in disguise                                     *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   free this object (C version)                         *
*************************************************************************/
void RPMDependency_Obj::FreeInternalRep(Tcl_Obj *objPtr)
{
   // Paranoia won't destroy ya - is this really a RPMDependency_Obj?
   assert(objPtr->typePtr == &mytype);

   RPMDependency_Obj *dis = (RPMDependency_Obj *)(objPtr->internalRep.otherValuePtr);
   dis->Dec_refcount();
}

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::DupInternalRep                    *
* ARGUMENTS     :   Source object, destination object                    *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a copy of an RPM dependency                   *
*************************************************************************/
void RPMDependency_Obj::DupInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
   if (srcPtr->typePtr != &mytype)
   {
      printf("Error: incoming type was %s not %s\n",srcPtr->typePtr->name,mytype.name);
      assert(srcPtr->typePtr == &mytype);
   }
   RPMDependency_Obj *from = (RPMDependency_Obj *)(srcPtr->internalRep.otherValuePtr);
   dupPtr->internalRep.otherValuePtr = from->Dup();
}

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::UpdateString                      *
* ARGUMENTS     :   Object to update                                     *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Update the string rep of the object                  *
*************************************************************************/
void RPMDependency_Obj::UpdateString(Tcl_Obj *objPtr)
{
   assert(objPtr->typePtr == &mytype);
   RPMDependency_Obj *dis = (RPMDependency_Obj *)(objPtr->internalRep.otherValuePtr);
   objPtr->bytes = dis->Get_stringrep(objPtr->length);
}

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::Get_Obj                           *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   Object with refcount of 0                            *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a Tcl_Obj from a header                       *
*************************************************************************/
Tcl_Obj *RPMDependency_Obj::Get_obj(void)
{
   Tcl_Obj *obj = Tcl_NewObj();
   obj->typePtr = &mytype;
   obj->internalRep.otherValuePtr = Dup();
   Tcl_InvalidateStringRep(obj);
   return obj;
}
/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::SetFromAny                        *
* ARGUMENTS     :   object to set from                                   *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a dependency from any object                  *
*************************************************************************/
int RPMDependency_Obj::SetFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
   // Handle simple case - this is a header object
   if (objPtr->typePtr == &mytype)
      return TCL_OK;
      
    return Cmd_base::Error(interp,"cannot create a %s from a %s\n",objPtr->typePtr->name,mytype.name);
}
/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::RPMDependency_Obj                 *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Set up an empty RPM dependency                       *
*************************************************************************/
RPMDependency_Obj::RPMDependency_Obj(void)
              :ref_count(1),dependency(0)
              
{
}
/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::~RPMDependency_Obj                *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Destroy an RPM dependency object                     *
*************************************************************************/
RPMDependency_Obj::~RPMDependency_Obj()
{
   assert(ref_count == 0);
   // Free strings as needed
   if (dependency)
      rpmdsUnlink(dependency,"RPMDependency_Obj::~RPMDependency_Obj");
}
/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::Dup_dependency                    *
* ARGUMENTS     :   ref to another problem                               *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a copy of a problem object                    *
*************************************************************************/
void RPMDependency_Obj::Dup_dependency(const rpmds x)
{
   dependency = x?rpmdsLink(x,"RPMDependency_Obj::RPM header RPMDependency_Obj"):0;
}
/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::RPMDependency_Obj                 *
* ARGUMENTS     :   ref to another object                                *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a copy of a header object                     *
*************************************************************************/
RPMDependency_Obj::RPMDependency_Obj(const RPMDependency_Obj &x)
              :ref_count(1)
{
   Dup_dependency(x.dependency);
}
/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::Create_from_ds                    *
* ARGUMENTS     :   ref to problem                                       *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a problem Tcl_Obj                             *
*************************************************************************/
Tcl_Obj *RPMDependency_Obj::Create_from_ds(const rpmds x)
{
   Tcl_Obj *y = Tcl_NewObj();
   assert(y);
   Tcl_IncrRefCount(y);
   Tcl_InvalidateStringRep(y);
   y->typePtr = &mytype;
   y->internalRep.otherValuePtr = new RPMDependency_Obj(x);
   return y;
}

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::RPMDependency_Obj                 *
* ARGUMENTS     :   ref to RPM dependency from library                   *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a header object                               *
*************************************************************************/
RPMDependency_Obj::RPMDependency_Obj(const rpmds x)
              :ref_count(1)
{
   Dup_dependency(x);
}

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::operator =                        *
* ARGUMENTS     :   Object to copy                                       *
* RETURNS       :   ref to this                                          *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Copy one header into another                         *
*************************************************************************/
RPMDependency_Obj &RPMDependency_Obj::operator =(const RPMDependency_Obj &x)
{
   Dup_dependency(x.dependency);
   return *this;
}

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::Get_stringrep                     *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   Tcl_Alloc'ed string rep of an object                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Return the string rep of an RPM dependency           *
*************************************************************************/
char *RPMDependency_Obj::Get_stringrep(int &len)
{
   // Get our parts as a TCL list
   Tcl_Obj *name    = Tcl_NewStringObj(rpmdsN(dependency),-1);
   Tcl_IncrRefCount(name);
   // we must return dynamaically allocated space, so allocate that
   int   size = 0;
   char *from = Tcl_GetStringFromObj(name,&size);
   char *space = Tcl_Alloc(size+1);
   assert(space);
   strncpy(space,from,size);
   space[size] = 0;
   Tcl_DecrRefCount(name);
   len = size;
   return space;
}

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::Inc_refcount                      *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   new ref count                                        *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Increment the refcount on a header object            *
*************************************************************************/
int RPMDependency_Obj::Inc_refcount(void)
{
   return ++ref_count;
}
/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::Dec_refcount                      *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   new refcount                                         *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Dec the refcount on an object.                       *
* NOTE          :   if the refcount goes to zero, the object will be     *
*                   FREED - DO NOT access this object after you have     *
*                   released your refcount!                              *
*************************************************************************/
int RPMDependency_Obj::Dec_refcount(void)
{
   int result = --ref_count;
   if (ref_count == 0)
      delete this;
   return result;
}

/*************************************************************************
* FUNCTION      :   RPMDependency_Obj::Get_exclusive                     *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   An exclusive reference for the object                *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Insure that no one else has a reference to this      *
*                   to this object - if somebody else has a ref, copy    *
*                   and return the copy.                                 *
*************************************************************************/
RPMDependency_Obj *RPMDependency_Obj::Get_exclusive(void)
{
   if (ref_count == 1)
      return this;
   RPMDependency_Obj *copy = new RPMDependency_Obj(*this);
   copy->Inc_refcount();
   return copy;
}
