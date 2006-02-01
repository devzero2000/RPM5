/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : transaction_set.cpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM transaction setfor TCL
 
This file implements an object type for TCL that contains an RPM header

*****************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "rpm_for_tcl.hpp"
#include "dependency.hpp"
#include <rpm/rpmts.h>
#include <rpm/rpmte.h>


const RPMTransaction_Set::Cmd_list RPMTransaction_Set::cmds[] =
{
 {{"help","Get help on subcommands"},&RPMTransaction_Set::Help},
 {{"destroy","Destroy transaction set"},&RPMTransaction_Set::Destroy},
 {{"install","Add an RPM to be installed"},&RPMTransaction_Set::Install},
 {{"add","Add an RPM to be upgraded"},&RPMTransaction_Set::Upgrade},
 {{"upgrade","Add an RPM to be upgraded"},&RPMTransaction_Set::Upgrade},
 {{"remove","Add an RPM to be removed"},&RPMTransaction_Set::Remove},
 {{"show","Show contents"},&RPMTransaction_Set::Show},
 {{"check","Check dependancies"},&RPMTransaction_Set::Check},
 {{"clear","clear all pending transactions"},&RPMTransaction_Set::Clear},
 {{"probflags","get or set problem mask flags"},&RPMTransaction_Set::ProbFlags},
 {{"order","sort packages by dependency"},&RPMTransaction_Set::Order},
 {{"test","do a test install"},&RPMTransaction_Set::Test},
 {{"run","do a REAL install (with rollback)"},&RPMTransaction_Set::Run},
 {{"problems","report current problem set"},&RPMTransaction_Set::Problems},

  {{0,0},0}
};

   	
#define BIT(x) {x,#x}
RPMTransaction_Set::Bits RPMTransaction_Set::bits[] =
{
    BIT(RPMCALLBACK_UNKNOWN),
    BIT(RPMCALLBACK_INST_PROGRESS),
    BIT(RPMCALLBACK_INST_START),
    BIT(RPMCALLBACK_INST_OPEN_FILE),
    BIT(RPMCALLBACK_INST_CLOSE_FILE),
    BIT(RPMCALLBACK_TRANS_PROGRESS),
    BIT(RPMCALLBACK_TRANS_START),
    BIT(RPMCALLBACK_TRANS_STOP),
    BIT(RPMCALLBACK_UNINST_PROGRESS),
    BIT(RPMCALLBACK_UNINST_START),
    BIT(RPMCALLBACK_UNINST_STOP),
    BIT(RPMCALLBACK_REPACKAGE_PROGRESS),
    BIT(RPMCALLBACK_REPACKAGE_START),
    BIT(RPMCALLBACK_REPACKAGE_STOP),
    BIT(RPMCALLBACK_UNPACK_ERROR),
    BIT(RPMCALLBACK_CPIO_ERROR)
};

RPMTransaction_Set::Bits RPMTransaction_Set::Prob_bits[] =\
{
    {RPMPROB_FILTER_NONE,"none"},
    {RPMPROB_FILTER_IGNOREOS,"ignoreos"},
    {RPMPROB_FILTER_IGNOREARCH,"ignorearch"},
    {RPMPROB_FILTER_REPLACEPKG,"replacepkgs"},
    {RPMPROB_FILTER_FORCERELOCATE,"badreloc"},
    {RPMPROB_FILTER_REPLACENEWFILES,"replacenewfiles"},
    {RPMPROB_FILTER_REPLACEOLDFILES,"replaceoldfiles"},
    {RPMPROB_FILTER_OLDPACKAGE,"oldpackage"},
    {RPMPROB_FILTER_DISKSPACE,"ignoresize"},
    {RPMPROB_FILTER_DISKNODES,"ignorenodes"},
    {(unsigned)(-1),"all"},
    {0,0}
};

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Create_transaction                      *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a transaction log                             *
*************************************************************************/
int RPM_for_TCL::Create_transaction(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc > 3)
   {
      Tcl_WrongNumArgs(interp,2,objv,"[name]");
      return TCL_ERROR;
   }

   // If no name, create one within the RPM namespace
   if (objc == 2)
   {
      for (int him = transaction_counter; transaction_counter < INT_MAX;him = ++transaction_counter)
      {
         char buffer[200];
         snprintf(buffer,sizeof(buffer),"::RPM::transaction%i",him);
         RPMTransaction_Set *ts = RPMTransaction_Set::Create(interp,buffer);
         if (ts)
            return TCL_OK;
      }
      return Error("Too many RPM transactions created. What are you, NUTS?!");
   }
   // Named transaction - try to create
   RPMTransaction_Set *ts = RPMTransaction_Set::Create(interp,objv[2]);
   if (!ts)
      return TCL_ERROR;
   return TCL_OK;
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Create                           *
* ARGUMENTS     :   interp, name                                         *
* RETURNS       :   object created or 0 if error                         *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Set up an empty transaction list                     *
*************************************************************************/
RPMTransaction_Set *RPMTransaction_Set::Create(Tcl_Interp *interp,const char *name)
{
   if (RPMTransaction_Set::Cmd_exists(interp,name))
   {
       RPM_for_TCL::Error(interp, "\"%s\" command already exists",name);
       return 0;
   }
   rpmts ts = rpmtsCreate();
   if (!ts)
   {
       RPM_for_TCL::Error(interp, "cannot create RPM transaction: %s",rpmErrorString());
       return 0;
   }

   return new RPMTransaction_Set(interp,name,ts);
}
   

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::RPMTransaction_Set               *
* ARGUMENTS     :   interp, name , transaction                           *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Set up an empty transaction list                     *
*************************************************************************/
RPMTransaction_Set::RPMTransaction_Set(Tcl_Interp *interp,const char *name,rpmts ts)
                   :Cmd_base(interp,name,&cmds[0].base,sizeof(cmds[0])),
                    transaction(ts),
                    header_list(0),
                    my_dirty_count(0),
                    prob_flags(rpmtsFilterFlags(transaction))
{
  // Init our DB dirty counter
  RPM_for_TCL::Are_DBs_dirty(my_dirty_count);
  rpmtsSetNotifyCallback(transaction,RPM_callback_entry,this);
  rpmtsSetSolveCallback(transaction,Solve,this);
  rpmtsSetRootDir(transaction,"/");
  OK(name);
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::~RPMTransaction_Set              *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Destroy an RPM transaction set                       *
*************************************************************************/
RPMTransaction_Set::~RPMTransaction_Set()
{
   // Free any RPM headers we have created to hand to the transaction list
   if (header_list)
      Tcl_DecrRefCount(header_list);
   header_list = 0;
   rpmtsFree(transaction);
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Exec                             *
* ARGUMENTS     :   which cmd to do,interp, arg count, arg list          *
* RETURNS       :   per subcommands                                      *
* EXCEPTIONS    :   Per subcommands                                      *
* PURPOSE       :   Carry out the RPM command                            *
*************************************************************************/
int RPMTransaction_Set::Exec(int which,Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   // Before doing any command, see if the databases are dirty - if so, close them
   if (RPM_for_TCL::Are_DBs_dirty(my_dirty_count))
      rpmtsCloseDB(transaction);
   return (this->*cmds[which].func)(interp,objc,objv);
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Destroy                          *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Destroy a transaction set                            *
*************************************************************************/
int RPMTransaction_Set::Destroy(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv," take no other parameters");
      return TCL_ERROR;
   }
   Delete();
   return TCL_OK;
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Install_or_remove                *
* ARGUMENTS     :   RPM headers to add                                   *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Add an RPM to an install set                         *
*************************************************************************/
int RPMTransaction_Set::Install_or_remove(Tcl_Obj *name,Install_mode mode)
{
   // Is this a list? if so, recurse through it
   Tcl_ObjType *listtype = Tcl_GetObjType("list");
   if (name->typePtr == listtype)
   {
       // OK, go recursive on this
       int count = 0;
       if (Tcl_ListObjLength(_interp,name,&count) != TCL_OK)
          return TCL_ERROR;

       for (int i = 0; i < count; ++i)
       {
          Tcl_Obj *element = 0;
          if (Tcl_ListObjIndex(_interp,name,i,&element) != TCL_OK)
          {
             return TCL_ERROR;
          }
          if (Install_or_remove(element,mode) != TCL_OK)
             return TCL_ERROR;
       }
       return TCL_OK;
   }
   // OK, so not a list. Try to make it into an RPM header
   if (Tcl_ConvertToType(_interp,name,&RPMHeader_Obj::mytype) != TCL_OK)
      return TCL_ERROR;
   RPMHeader_Obj *header = ( RPMHeader_Obj *)(name->internalRep.otherValuePtr);\
   // Unfortunately, the transaction set API does not give us a way to know when
   // it has freed a fnpyKey key object. In order to clean these up, we will create
   // a TCL list object of all headers we use for this purpose, and clean it as needed.
   Tcl_Obj *hdr_copy = header->Get_obj();
   Tcl_IncrRefCount(hdr_copy);
   
   int error = 0;
   switch (mode)
   {
       case INSTALL:
          error = rpmtsAddInstallElement(transaction,*header,header,0,0);
       break;
       case UPGRADE:
          error = rpmtsAddInstallElement(transaction,*header,header,1,0);
       break;
       case REMOVE:
          error = rpmtsAddEraseElement(transaction,*header,header->DB_entry());
       break;
   }
   
   switch (error)
   {
      case 0:
         // Record that we have created an entry on the list
         header_list = Grow_list(header_list,hdr_copy);
      return TCL_OK;
      
      case 1:
         header->Dec_refcount();
      return Error("Error adding %s: %s\n",Tcl_GetStringFromObj(name,0),rpmErrorString());
      
      case 2:
         header->Dec_refcount();
      return Error("Error adding %s: needs capabilities %s\n",Tcl_GetStringFromObj(name,0),rpmErrorString());
      
      default:
         header->Dec_refcount();
      return Error("Unknown RPMlib error %d adding %s: needs capabilities %s\n",error,Tcl_GetStringFromObj(name,0),rpmErrorString());
   } 	
   return TCL_OK;
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Install                          *
* ARGUMENTS     :   RPM headers to add                                   *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Add a file to install                                *
*************************************************************************/
int RPMTransaction_Set::Install(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc < 3)
   {
      Tcl_WrongNumArgs(interp,2,objv,"header [header...]");
      return TCL_ERROR;
   }
   // Build a list of indexes matching the packages given.
   Tcl_Obj *args = Tcl_NewListObj(objc-2,objv+2);
   if (!args)
      return Error("Cannot concat arglist!");
   Tcl_IncrRefCount(args);
   int rc = Install_or_remove(args,INSTALL);
   Tcl_DecrRefCount(args);
   return rc;
}
/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Upgrade                          *
* ARGUMENTS     :   RPM headers to add                                   *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Add a file to upgrade                                *
*************************************************************************/
int RPMTransaction_Set::Upgrade(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc < 3)
   {
      Tcl_WrongNumArgs(interp,2,objv,"header [header...]");
      return TCL_ERROR;
   }
   // Build a list of indexes matching the packages given.
   Tcl_Obj *args = Tcl_NewListObj(objc-2,objv+2);
   if (!args)
      return Error("Cannot concat arglist!");
   Tcl_IncrRefCount(args);
   int rc = Install_or_remove(args,UPGRADE);
   Tcl_DecrRefCount(args);
   return rc;
}
/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Remove                           *
* ARGUMENTS     :   RPM headers to add                                   *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Destroy a transaction set                            *
*************************************************************************/
int RPMTransaction_Set::Remove(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc < 3)
   {
      Tcl_WrongNumArgs(interp,2,objv,"header [header...]");
      return TCL_ERROR;
   }
   // Build a list of indexes matching the packages given.
   Tcl_Obj *args = Tcl_NewListObj(objc-2,objv+2);
   if (!args)
      return Error("Cannot concat arglist!");
   int rc = Install_or_remove(args,REMOVE);
   Tcl_DecrRefCount(args);
   return rc;
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Show                             *
* ARGUMENTS     :   Object to look for                                   *
* RETURNS       :   List of packages found                               *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Show the contents of a transaction set               *
*************************************************************************/
Tcl_Obj *RPMTransaction_Set::Show(Tcl_Obj *item)
{
   Tcl_Obj *sub_obj = Tcl_NewObj();
   Tcl_IncrRefCount(sub_obj);
   // Is this a list?
   if (item && item->typePtr == listtype)
   {
       // OK, go recursive on this
       int count = 0;
       if (Tcl_ListObjLength(_interp,item,&count) != TCL_OK)
          return sub_obj;

       for (int i = 0; i < count; ++i)
       {
          Tcl_Obj *element = 0;
          if (Tcl_ListObjIndex(_interp,item,i,&element) != TCL_OK)
          {
             return sub_obj;
          }
          Tcl_ListObjAppendElement(_interp,sub_obj,Show(element));
       }
       return sub_obj;
   }
   // OK, not a list. were we given ANYTHING? if not, get everything.
   void *name = 0;
   if (item)
   {
      if (item->typePtr == &RPMHeader_Obj::mytype)
      {
         RPMHeader_Obj *header = ( RPMHeader_Obj *)(item->internalRep.otherValuePtr);
         int size = 0;
         int type = 0;
         if (!header->GetEntry(RPMTAG_NAME,type,name,size))
            return sub_obj;
      }
      else // Not a header, interp as a string
      {
         name = (void *)Tcl_GetStringFromObj(item,0);
      }
   }
   rpmtsi matches = rpmtsiInit(transaction);
   if (!matches)
      return sub_obj;
   
   // OK, go over the list and create a list of items to return
  
   for(;;)
   {
     rpmte te = rpmtsiNext(matches,(rpmElementType)0);
     if (!te)
        break;
     
     Tcl_Obj *results[2];
     
     switch (rpmteType(te))
     {
        case TR_ADDED:
        {
           RPMHeader_Obj *hdr = (RPMHeader_Obj *)rpmteKey(te); 	
           results[0] = Tcl_NewStringObj("add",-1);
           results[1] = hdr->Get_obj();;
        }
        break;
        
        case TR_REMOVED:
        {
           results[0] = Tcl_NewStringObj("remove",-1);
           results[1] = Tcl_NewStringObj(rpmteN(te),-1);
        }
        break;
     }

     Tcl_Obj *list = Tcl_NewListObj(2,results);
     Tcl_IncrRefCount(list);
     Tcl_ListObjAppendElement(_interp,sub_obj,list);
   }
   rpmtsiFree(matches);
   return sub_obj;
}
/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Show                             *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Show the contents of a transaction set               *
*************************************************************************/
int RPMTransaction_Set::Show(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   Tcl_Obj *args    = 0;
   // OK, go over the list and create a list of items to return
   if (objc > 2)
   {
      // Build a list of indexes matching the packages given.
      Tcl_Obj *args = Tcl_NewListObj(objc-2,objv+2);
      if (!args)
         return Error("Cannot concat arglist!");
      Tcl_IncrRefCount(args);
   }

   Tcl_Obj *result = Show(args);
   // Free arg list, if created.
   if (args)
      Tcl_DecrRefCount(args);
   return OK(result);
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Check                            *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Check the transaction set for problems               *
*************************************************************************/
int RPMTransaction_Set::Check(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,0);
      return TCL_ERROR;
   }

   rpmtsCheck(transaction);
   return Problems(interp);
}
/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Problems                         *
* ARGUMENTS     :   interp                                               *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Return current problem set as TCL list               *
*************************************************************************/
int RPMTransaction_Set::Problems(Tcl_Interp *interp,int , Tcl_Obj *const [])
{
   rpmps problems = rpmtsProblems(transaction);
   int num_probs      = rpmpsNumProblems(problems);
   Tcl_Obj *rv = 0;
   for (int i = 0; i < num_probs; ++i)
   {
      rpmProblem what = &problems->probs[i];
      rv = Grow_list(rv,RPMPRoblem_Obj::Create_from_problem(*what));
   }
   return OK(rv);
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Test                             *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Check the transaction set for problems               *
*************************************************************************/
int RPMTransaction_Set::Test(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,0);
      return TCL_ERROR;
   }
   rpmtsSetFlags(transaction,(rpmtransFlags)(RPMTRANS_FLAG_TEST ));

   int status = rpmtsRun(transaction,0,(rpmprobFilterFlags)prob_flags);
   // Assume we've dirtied the database
   RPM_for_TCL::DBs_are_dirty();
   if (status == 0)
     return TCL_OK;
   else if (status < 0)
     return Error("Error on test: %d %s",status,rpmErrorString());

   return Problems(interp);
}
/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Run                              *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Do the install for real                              *
*************************************************************************/
int RPMTransaction_Set::Run(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,0);
      return TCL_ERROR;
   }
   rpmtsSetFlags(transaction,(rpmtransFlags)(RPMTRANS_FLAG_NONE ));

   int status = rpmtsRun(transaction,0,(rpmprobFilterFlags)prob_flags);
   // Assume we've dirtied the databases
   RPM_for_TCL::DBs_are_dirty();
   
   if (status == 0)
     return TCL_OK;
   else if (status < 0)
     return Error("Error on install: %d %s",status,rpmErrorString());
   
   return Problems(interp);
}
/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Clear                            *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Clear the list of transactions                       *
*************************************************************************/
int RPMTransaction_Set::Clear(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,0);
      return TCL_ERROR;
   }
   rpmtsEmpty(transaction);
   // Free any header objects we've created
   if (header_list)
   {
      Tcl_DecrRefCount(header_list);
      header_list = 0;
   }
   return TCL_OK;
}
/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::ProbFlags                        *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Set or get problem mask flags                        *
*************************************************************************/
int RPMTransaction_Set::ProbFlags(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc >= 3)
   {
      // Build a list of indexes matching the packages given.
      Tcl_Obj *args = Tcl_NewListObj(objc-2,objv+2);
      if (!args)
         return Error("Cannot concat arglist!");
      Tcl_IncrRefCount(args);
      // Iterate over list and build up the list
      
      unsigned mask = prob_flags;
      int count = 0;
      if (Tcl_ListObjLength(interp,args,&count) != TCL_OK)
      {
parse_error:
         Tcl_DecrRefCount(args);
         return TCL_ERROR;
      }
      for (int i = 0; i < count; ++i)
      {
         Tcl_Obj *flag = 0;
         int which = 0;
         if (Tcl_ListObjIndex(interp,args,i,&flag) != TCL_OK)
             goto parse_error;
             
         if (Tcl_GetIndexFromObjStruct(interp,flag,(char **)&Prob_bits[0].msg,sizeof(Prob_bits[0]),
                                 "flag",0,&which
                                ) != TCL_OK)
             goto parse_error;
         if (Prob_bits[which].bit == RPMPROB_FILTER_NONE )
            mask = RPMPROB_FILTER_NONE;
         else
            mask |= Prob_bits[which].bit;
      }
      Tcl_DecrRefCount(args);
      prob_flags = mask;
   }
   // Now, build the return list
   Tcl_Obj *val = Tcl_NewObj();
   Tcl_IncrRefCount(val);
   if (prob_flags == 0)
   {
      if (Tcl_ListObjAppendElement(interp,val,Tcl_NewStringObj(Prob_bits[0].msg,-1)) != TCL_OK)
      {
out_err:
         Tcl_DecrRefCount(val);
         return TCL_ERROR;
      }
   }
   else if (prob_flags == (unsigned)(-1))
   {
      if (Tcl_ListObjAppendElement(interp,val,Tcl_NewStringObj("all",-1)) != TCL_OK)
      {
         goto out_err;
      }
   }
   else
   {
      for (int i = 0; Prob_bits[i].msg; ++i)
      {
          if (Prob_bits[i].bit == (unsigned)(-1))
             continue;
             
          if (prob_flags & Prob_bits[i].bit)
          {
             if (Tcl_ListObjAppendElement(interp,val,Tcl_NewStringObj(Prob_bits[i].msg,-1)) != TCL_OK)
             {
                 Tcl_DecrRefCount(val);
                 return TCL_ERROR;
             }
          }
      }
   }
   
   return OK(val);
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::RPM_callback_entry               *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   progress callback                                    *
*************************************************************************/
void *RPMTransaction_Set::RPM_callback_entry( const void * h, 
                                             const rpmCallbackType what,
                                             const unsigned long amount,
                                             const unsigned long total,
                                             fnpyKey key,
                                             rpmCallbackData data
                                           )
{
  return ((RPMTransaction_Set *)data)->RPM_callback(h,what,amount,total,key);
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Solve                            *
* ARGUMENTS     :   transaction set, key, this in disguise               *
* RETURNS       :   0                                                    *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Solve an install problem                             *
*************************************************************************/
int RPMTransaction_Set::Solve(rpmts , rpmds key, const void *data)
{
  return ((RPMTransaction_Set *)data)->Solve(key);
}

/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::RPM_callback                     *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Set or get problem mask flags                        *
*************************************************************************/
void *RPMTransaction_Set::RPM_callback( const void * h, 
                                             const rpmCallbackType what,
                                             const unsigned long amount,
                                             const unsigned long total,
                                             fnpyKey key
                                           )
{
   // Build up the list of call back bits
   Tcl_Obj *bitstring = Tcl_NewObj();
   for (unsigned i = 0; i < sizeof(bits)/sizeof(bits[0]);++i)
   {
      if (what & bits[i].bit)
      {
         Tcl_ListObjAppendElement(_interp,bitstring,Tcl_NewStringObj(bits[i].msg,-1));
      }
   }
   RPMHeader_Obj *hdr = (RPMHeader_Obj *)key;
   Tcl_Obj *cmd[] = 
   {
      Tcl_NewStringObj("::RPM::Callback",-1),
      bitstring,
      Tcl_NewLongObj(amount),
      Tcl_NewLongObj(total),
      hdr?hdr->Get_obj():Tcl_NewObj()
   };
   Tcl_Obj *script = Tcl_NewListObj(sizeof(cmd)/sizeof(cmd[0]),cmd);
   Tcl_IncrRefCount(script);
   Tcl_EvalObj(_interp,script);
   Tcl_DecrRefCount(script);

   // Now, handle any special operations here
   if (what & RPMCALLBACK_INST_OPEN_FILE)
   {
		assert(hdr);
		FD_t fd = hdr->Open();
		if (!fd || Ferror(fd))
		{
			if (fd)
			{
			   Fclose(fd);
			   fd = 0;
			}
		}
		return (void *)fd;
   }
   if (what & RPMCALLBACK_INST_CLOSE_FILE)
   {
		assert(hdr);
		hdr->Close();
		return 0;
   }
   return 0;
}
/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Solve                            *
* ARGUMENTS     :   transaction set, key, this in disguise               *
* RETURNS       :   -1 (retry), 0 (ignore), 1 (not found)                *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Solve an install problem                             *
*************************************************************************/
int RPMTransaction_Set::Solve(rpmds key)
{
   // Throw the key over to TCL.
   Tcl_Obj *cmd[] = 
   {
      Tcl_NewStringObj("::RPM::Solve",-1),
      Tcl_NewStringObj(Tcl_GetCommandName(_interp, me),-1),
      RPMDependency_Obj::Create_from_ds(key)
   };
   Tcl_Obj *script = Tcl_NewListObj(sizeof(cmd)/sizeof(cmd[0]),cmd);
   Tcl_IncrRefCount(script);
   Tcl_EvalObj(_interp,script);
   Tcl_DecrRefCount(script);
   // Now get the return value
   Tcl_Obj *result = Tcl_GetObjResult(_interp);
   int rv = 0;
   Tcl_GetIntFromObj(_interp,result,&rv);
   return rv;
}
/*************************************************************************
* FUNCTION      :   RPMTransaction_Set::Order                            *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Order set for install                                *
*************************************************************************/
int RPMTransaction_Set::Order(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,0);
      return TCL_ERROR;
   }
   int pkgs = rpmtsOrder(transaction);
   if (pkgs > 0)
      return Error("%d packages could not be ordered",pkgs);
   return TCL_OK;
}
