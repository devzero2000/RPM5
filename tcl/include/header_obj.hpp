#ifndef HEADER_OBJ_HPP
#define HEADER_OBJ_HPP
/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : heder_obj.hpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM header object for TCL
 
This file defines a new object type for TCL - the RPM header object.


*****************************************************************************/
#include <tcl.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <rpm/header.h>

// the RPMHeader_Obj will be the internal representation of the TCL object,
// stored in the internalRep.otherValuePtr of the Tcl_Obj
class RPMHeader_Obj
{
// The needed procs and structs to interface to TCL
   struct MyType: public Tcl_ObjType
   {
      MyType(void);
      ~MyType();
   };
public:
   static MyType mytype;
private:
   
   static void FreeInternalRep(Tcl_Obj *objPtr);
   static void DupInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);
   static void UpdateString(Tcl_Obj *objPtr);
   static int  SetFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
   
   mutable int ref_count;
   Header header;
   const char *filename; // if from a file, this is the file it is from
   int         db_entry; // Database entry # or -1 if not from DB
   FD_t        fd;       // RPM style fd opened for object
   int_32      transaction_time; // Time object was read from file
   Tcl_Obj *Item_to_object(const void *item,int size,int type);
   static const char *DupFname(const char *filename);
   
public:
   static Tcl_Obj *Create_from_header(const Header x,const char *filename=0,int db_entry=-1,int trans_time = 0);
   Tcl_Obj *Get_obj(void);
   RPMHeader_Obj(void);
   RPMHeader_Obj(const RPMHeader_Obj &x);
   RPMHeader_Obj(const Header x,const char *filename = 0,int db_entry=-1);
   ~RPMHeader_Obj();
   
   const char *Filename(void) { return filename;}
   int DB_entry(void)         { return db_entry;}
   RPMHeader_Obj &operator =(const RPMHeader_Obj &x);
   
   char *Get_stringrep(int &len);
   
   // Reference counting routines
   int Inc_refcount(void);
   int Dec_refcount(void);
   RPMHeader_Obj *Get_exclusive(void);
   RPMHeader_Obj *Dup(void) { Inc_refcount(); return this;}
   
   // Header data manipulation
   Tcl_Obj *HeaderEntry(int tag);
   Tcl_Obj *TagType(int tag);
   Tcl_Obj *HeaderNextIterator(HeaderIterator iterator,int &tag);
   operator Header(void) { return header;}
   
   int GetEntry(int tag,int &type,void *&item,int &size);
   
   // File manipulation
   FD_t Open(void);
   void Close(void);
   
   // Timestamp manipulation
   int_32 Get_transaction_time(void)     { return transaction_time;}
};

#endif

