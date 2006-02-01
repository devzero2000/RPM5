/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : header_obj.cpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM header object for TCL
 
This file implements an object type for TCL that contains an RPM header

*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include "rpm_for_tcl.hpp"
#include <stdarg.h>
#include <errno.h>

// C-style "vftable" for this class - gosh, it would be soo much easier were TCL
// written in C++....

RPMHeader_Obj::MyType RPMHeader_Obj::mytype;


/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::MyType::MyType                        *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Register type with TCL                               *
*************************************************************************/
RPMHeader_Obj::MyType::MyType(void)
{
   name = "RPMHeader";
   freeIntRepProc   = RPMHeader_Obj::FreeInternalRep;
   dupIntRepProc    = RPMHeader_Obj::DupInternalRep;
   updateStringProc = RPMHeader_Obj::UpdateString;
   setFromAnyProc   = RPMHeader_Obj::SetFromAny;
   Tcl_RegisterObjType(this);
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::MyType::~MyType                       *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   deregister type with TCL                             *
*************************************************************************/
RPMHeader_Obj::MyType::~MyType()
{
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::FreeInternalRep                       *
* ARGUMENTS     :   this in disguise                                     *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   free this object (C version)                         *
*************************************************************************/
void RPMHeader_Obj::FreeInternalRep(Tcl_Obj *objPtr)
{
   // Paranoia won't destroy ya - is this really a RPMHeader_Obj?
   assert(objPtr->typePtr == &mytype);
   RPMHeader_Obj *dis = (RPMHeader_Obj *)(objPtr->internalRep.otherValuePtr);
   dis->Dec_refcount();
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::DupInternalRep                        *
* ARGUMENTS     :   Source object, destination object                    *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a copy of an RPM header                       *
*************************************************************************/
void RPMHeader_Obj::DupInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
   if (srcPtr->typePtr != &mytype)
   {
      printf("Error: incoming type was %s not %s\n",srcPtr->typePtr->name,mytype.name);
      assert(srcPtr->typePtr == &mytype);
   }
   RPMHeader_Obj *from = (RPMHeader_Obj *)(srcPtr->internalRep.otherValuePtr);
   dupPtr->internalRep.otherValuePtr = from->Dup();
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::UpdateString                          *
* ARGUMENTS     :   Object to update                                     *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Update the string rep of the object                  *
*************************************************************************/
void RPMHeader_Obj::UpdateString(Tcl_Obj *objPtr)
{
   assert(objPtr->typePtr == &mytype);
   RPMHeader_Obj *dis = (RPMHeader_Obj *)(objPtr->internalRep.otherValuePtr);
   objPtr->bytes = dis->Get_stringrep(objPtr->length);
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::Get_Obj                               *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   Object with refcount of 0                            *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a Tcl_Obj from a header                       *
*************************************************************************/
Tcl_Obj *RPMHeader_Obj::Get_obj(void)
{
   Tcl_Obj *obj = Tcl_NewObj();
   obj->typePtr = &mytype;
   obj->internalRep.otherValuePtr = Dup();
   Tcl_InvalidateStringRep(obj);
   return obj;
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::SetFromAny                            *
* ARGUMENTS     :   object to set from                                   *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a header from any object                      *
*************************************************************************/
int RPMHeader_Obj::SetFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
   // Handle simple case - this is a header object
   if (objPtr->typePtr == &mytype)
      return TCL_OK;

   // Stupid check - is this a list? if so, get first element
   if (objPtr->typePtr == Cmd_base::listtype)
   {
      Tcl_Obj *element = objPtr;
      while (element->typePtr == Cmd_base::listtype)
      {
	     if (Tcl_ListObjIndex(interp,element,0,&element) != TCL_OK)
	     {
	        return TCL_ERROR;
	     }
      }
      // If the first element is of my type, use it
      if (element->typePtr == &mytype)
      {
         // Since the list is going bye-bye, grab the header data now
         RPMHeader_Obj *from = (RPMHeader_Obj *)(element->internalRep.otherValuePtr);
         from = from->Dup();
         objPtr->typePtr->freeIntRepProc(objPtr);
         objPtr->typePtr = &mytype;
         objPtr->internalRep.otherValuePtr = from;
         Tcl_InvalidateStringRep(objPtr);
         return TCL_OK;
      }
   }

   // OK, we need to try to find this RPM. Toss this over to the RPM
   // command
   Tcl_Obj *cmd[2] = 
   {
      Tcl_NewStringObj("::RPM::Find_package",-1),
      Tcl_DuplicateObj(objPtr)
   };
   Tcl_IncrRefCount(cmd[0]);
   Tcl_IncrRefCount(cmd[1]);
   
   int result = Tcl_EvalObjv(interp,2,cmd,0);
   Tcl_DecrRefCount(cmd[0]);
   Tcl_DecrRefCount(cmd[1]);
   
   if (result != TCL_OK)
      return result;
   // OK, so store the result into the object
   if (objPtr->typePtr && objPtr->typePtr->freeIntRepProc)
       objPtr->typePtr->freeIntRepProc(objPtr);
   Tcl_Obj *res = Tcl_GetObjResult(interp);
   // Dis-assemble any list objects
   while (res->typePtr == Cmd_base::listtype)
   {
       if (Tcl_ListObjIndex(interp,res,0,&res) != TCL_OK)
       {
          return TCL_ERROR;
       }
   }
   if (res->typePtr != &mytype)
   {
      return Cmd_base::Error(interp,"RPM::Find_package returned a type %s not %s\n",res->typePtr?res->typePtr->name:"<null obj>",mytype.name);
   }
   objPtr->typePtr = &mytype;
   DupInternalRep(res,objPtr);
   Tcl_InvalidateStringRep(objPtr);
   return TCL_OK;
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::RPMHeader_Obj                         *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Set up an empty RPM header                           *
*************************************************************************/
RPMHeader_Obj::RPMHeader_Obj(void)
              :ref_count(1),
               header(0),
               filename(0),
               db_entry(-1),
               fd(0)
{
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::~RPMHeader_Obj                        *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Destroy an RPM header object                         *
*************************************************************************/
RPMHeader_Obj::~RPMHeader_Obj()
{
   assert(ref_count == 0);

   // Free the header from the RPM library
   if (header)
      headerUnlink(header);
   header = 0;
   if (filename)
      delete [] filename;
   filename = 0;
   Close();
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::DupFname                              *
* ARGUMENTS     :   ref to another object                                *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a copy of a header object                     *
*************************************************************************/
const char *RPMHeader_Obj::DupFname(const char *fname)
{
   if (!fname)
      return 0;
   int len = strlen(fname);
   char *fn = new char[len+1];
   strncpy(fn,fname,len);
   fn[len] = 0;
   return fn;
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::RPMHeader_Obj                         *
* ARGUMENTS     :   ref to another object                                *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a copy of a header object                     *
*************************************************************************/
RPMHeader_Obj::RPMHeader_Obj(const RPMHeader_Obj &x)
              :ref_count(1),
               header(headerLink(x.header)),
               filename(DupFname(x.filename)),
               db_entry(x.db_entry),
               fd(x.fd?(fdLink(x.fd,"copy ctor"),x.fd):0)
{
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::Create_from_header                    *
* ARGUMENTS     :   ref to RPM header from library, file name (optional) *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a header Tcl_Obj                              *
*************************************************************************/
Tcl_Obj *RPMHeader_Obj::Create_from_header(const Header x,const char *filename,
                                           int db_entry,int trans_time
                                          )
{

   Tcl_Obj *y = Tcl_NewObj();
   assert(y);
   Tcl_IncrRefCount(y);
   if (y->typePtr && y->typePtr->freeIntRepProc)
       y->typePtr->freeIntRepProc(y);
   Tcl_InvalidateStringRep(y);
   y->typePtr = &mytype;
   RPMHeader_Obj *hdr = new RPMHeader_Obj(x,filename,db_entry);
   hdr->transaction_time = trans_time;
   y->internalRep.otherValuePtr = hdr;
   return y;
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::RPMHeader_Obj                         *
* ARGUMENTS     :   ref to RPM header from library                       *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a header object                               *
*************************************************************************/
RPMHeader_Obj::RPMHeader_Obj(const Header x,const char *fname,int _db)
              :ref_count(1),
               header(headerLink(x)),
               filename(DupFname(fname)),
               db_entry(_db),
               fd(0)
{
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::operator =                            *
* ARGUMENTS     :   Object to copy                                       *
* RETURNS       :   ref to this                                          *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Copy one header into another                         *
*************************************************************************/
RPMHeader_Obj &RPMHeader_Obj::operator =(const RPMHeader_Obj &x)
{
   if (header)
      headerFree(header);
   header = headerLink(x.header);
   return *this;
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::Get_stringrep                         *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   Tcl_Alloc'ed string rep of an object                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Return the string rep of an RPM header               *
*************************************************************************/
char *RPMHeader_Obj::Get_stringrep(int &len)
{
   // Define the default return string value
   Tcl_Obj *name    = 0;
   
   if (header)
   {
      // OK, so we actually HAVE an RPM selected. Get it's name and version
      name    = HeaderEntry(RPMTAG_NAME);
      assert(name);
   }
   else
   {
      name    = Tcl_NewStringObj("<no RPM selected>",-1);
      Tcl_IncrRefCount(name);
   }

   // we must return dynmaically allocated space, so allocate that
   int size = 0;
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
* FUNCTION      :   RPMHeader_Obj::TagType                               *
* ARGUMENTS     :   tag                                                  *
* RETURNS       :   Null if error, else object                           *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given the header and the tag to extract, return a    *
*                   Tcl_Obj representation of the TYPE of the tag        *
* NOTE          :   The object so returned will have a refcount of 1     *
*************************************************************************/
Tcl_Obj *RPMHeader_Obj::TagType(int tag)
{
   void *item     = 0;
   int  size      = 0;
   int  type      = 0;
   Tcl_Obj *value = 0;
   if (headerGetEntry(header,tag,&type,&item,&size))
   {
    switch (type)
    {
        case RPM_I18NSTRING_TYPE:
           value = Tcl_NewStringObj("RPM_I18NSTRING_TYPE",-1);
        break;

        case RPM_STRING_TYPE:
           value = Tcl_NewStringObj("RPM_STRING_TYPE",-1);
        break;

        case RPM_CHAR_TYPE:
           value = Tcl_NewStringObj("RPM_CHAR_TYPE",-1);
        break;
        
        case RPM_INT16_TYPE:
           value = Tcl_NewStringObj("RPM_INT16_TYPE",-1);
        break;
        
        case RPM_INT32_TYPE:
           value = Tcl_NewStringObj("RPM_INT32_TYPE",-1);
        break;
        
#ifdef RPM_INT64_TYPE_IS_SUPPORTED
        case RPM_INT64_TYPE:
           value = Tcl_NewStringObj("RPM_INT64_TYPE",-1);
        break;
#endif
        
        case RPM_STRING_ARRAY_TYPE:
           value = Tcl_NewStringObj("RPM_STRING_ARRAY_TYPE",-1);
        break;
        
        case RPM_BIN_TYPE:
           value = Tcl_NewStringObj("RPM_BIN_TYPE",-1);
        break;
        
        default:
           value = Tcl_NewLongObj(type);
        break;
    }
    Tcl_IncrRefCount(value);
   }
   return value;
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::GetEntry                              *
* ARGUMENTS     :   tag                                                  *
* RETURNS       :   data, type, size                                     *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given the header and the tag to extract, return a    *
*                   Tcl_Obj representation                               *
* NOTE          :   The object so returned will have a refcount of 1     *
*************************************************************************/
int RPMHeader_Obj::GetEntry(int tag,int &type,void *&item,int &size)
{
   return headerGetEntry(header,tag,&type,&item,&size);
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::HeaderEntry                           *
* ARGUMENTS     :   tag                                                  *
* RETURNS       :   Null if error, else object                           *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given the header and the tag to extract, return a    *
*                   Tcl_Obj representation                               *
* NOTE          :   The object so returned will have a refcount of 1     *
*************************************************************************/
Tcl_Obj *RPMHeader_Obj::HeaderEntry(int tag)
{
   void *item     = 0;
   int  size      = 0;
   int  type      = 0;
   Tcl_Obj *value = 0;
   if (GetEntry(tag,type,item,size))
   {
      value = Item_to_object(item,size,type);
   }
   return value;
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::HeaderNextIterator                    *
* ARGUMENTS     :   header iterator                                      *
* RETURNS       :   Null if error, else object                           *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given an RPM header iterator, return next object     *
* NOTE          :   The object so returned will have a refcount of 1     *
*************************************************************************/
Tcl_Obj *RPMHeader_Obj::HeaderNextIterator(HeaderIterator iterator,int &tag)
{
   const void *item     = 0;
   int  size      = 0;
   int  type      = 0;
   Tcl_Obj *value = 0;
   if (headerNextIterator(iterator,&tag,&type,&item,&size))
   {
      value = Item_to_object(item,size,type);
   }
   return value;
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::Item_to_object                        *
* ARGUMENTS     :   item pointer, size of object, type of object         *
* RETURNS       :   Null if error, else object                           *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given the results from a header query, make a TCL obj*
* NOTE          :   The object so returned will have a refcount of 1     *
*************************************************************************/
Tcl_Obj *RPMHeader_Obj::Item_to_object(const void *item,int size,int type)
{
    Tcl_Obj *value = 0;

    switch (type)
    {
        case RPM_I18NSTRING_TYPE:
        {
           char **ptr = (char **)item;
           if (size == 1)
           {
              value = Tcl_NewStringObj(ptr[0],-1);
           }
           else
           {
              value = Tcl_NewObj();
              for (int i = 0; i < size; ++i)
              {
                 Tcl_ListObjAppendElement(0,value,Tcl_NewStringObj(ptr[i],-1));
              }
           }
        }
        break;

        case RPM_STRING_TYPE:
        {
           value = Tcl_NewStringObj((char *)item,-1);
        }
        break;

        case RPM_CHAR_TYPE:
        {
           value = Tcl_NewStringObj((char *)item,1);
        }
        break;
        
        case RPM_INT16_TYPE:
        {
           value = Tcl_NewIntObj((*(int *)item)&0xFFFF);
        }
        break;
        
        case RPM_INT32_TYPE:
        {
           value = Tcl_NewIntObj(*(int *)item);
        }
        break;
        
#ifdef RPM_INT64_TYPE_IS_SUPPORTED
        case RPM_INT64_TYPE:
        {
           value = Tcl_NewWideIntObj(*(long long *)item);
        }
        break;
#endif
        
        case RPM_STRING_ARRAY_TYPE:
        {
		    value = Tcl_NewObj();
		    const char **strings = (const char **)item;
		    for (int i = 0; i < size; ++i)
		    {
		        Tcl_ListObjAppendElement(0,value,Tcl_NewStringObj(strings[i],-1));
		    }
        }
        break;
        
        case RPM_BIN_TYPE:
           value = Tcl_NewStringObj((char *)item,size);
        break;
        
        default:
            printf("type %d size %d\n",type,size);
        break;
    }
	headerFreeData(item,(rpmTagType)type);
    if (value)
       Tcl_IncrRefCount(value);
    return value;
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::Inc_refcount                          *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   new ref count                                        *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Increment the refcount on a header object            *
*************************************************************************/
int RPMHeader_Obj::Inc_refcount(void)
{
   return ++ref_count;
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::Dec_refcount                          *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   new refcount                                         *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Dec the refcount on an object.                       *
* NOTE          :   if the refcount goes to zero, the object will be     *
*                   FREED - DO NOT access this object after you have     *
*                   released your refcount!                              *
*************************************************************************/
int RPMHeader_Obj::Dec_refcount(void)
{
   int result = --ref_count;
   if (ref_count == 0)
      delete this;
   return result;
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::Get_exclusive                         *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   An exclusive reference for the object                *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Insure that no one else has a reference to this      *
*                   to this object - if somebody else has a ref, copy    *
*                   and return the copy.                                 *
*************************************************************************/
RPMHeader_Obj *RPMHeader_Obj::Get_exclusive(void)
{
   if (ref_count == 1)
      return this;
   RPMHeader_Obj *copy = new RPMHeader_Obj(*this);
   copy->Inc_refcount();
   return copy;
}

/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::Open                                  *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   RPM-type file descriptor                             *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Open an RPM file                                     *
*************************************************************************/
FD_t RPMHeader_Obj::Open(void)
{
   const char *fname = filename;
   // Get the name of the RPM
   void *item     = 0;
   int  size      = 0;
   int  type      = 0;
   GetEntry(RPMTAG_NAME,type,item,size);
   const char *name = (const char *)item;
   
   if (!filename) // If no file know, check for cache
   {
      printf("no filename - check cache\n");
      if (GetEntry(RPMTAG_CACHEPKGPATH,type,item,size))
      {
         printf("cache is %s\n",(char *)item);
         fname = ((const char **)item)[0];
      }
   }
   if (!fname)
   {
      rpmlog(RPMLOG_ERR,"Cannot open package %s - no filename known",name);
      return 0;
   }
   fd = Fopen(fname, "r.ufdio");
   if (!fd)
   {
      char buff[100];
      strerror_r(errno,buff,sizeof(buff));
      rpmlog(RPMLOG_ERR,"Cannot open package %s - $s",name,buff);
      return 0;
   }
   fdLink(fd, "RPMHeader_Obj::Open");
   return fd;
}
/*************************************************************************
* FUNCTION      :   RPMHeader_Obj::Close                                 *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Close an RPM file                                    *
*************************************************************************/
void RPMHeader_Obj::Close(void)
{
   if (!fd)
      return;
   fdFree(fd, "RPMHeader_Obj::Close");
   fd = 0;
}
