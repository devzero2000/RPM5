/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : rpm_for_tcl.cpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM bindings for TCL
 
This file implements a verb for the TCL language to allow manipulating
RedHat Package Manager (RPM) files and databases.

*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include "rpm_for_tcl.hpp"
#include <rpm/rpmts.h>
#include <rpm/rpmmacro.h>
#include "config.h"
#include "version.hpp"

extern "C"  // allow 'init' to be called from old-style C
{
   int 	Rpm_Init(Tcl_Interp *interp);
}

struct RPM_for_TCL::Tag_list
{
   char *name;
   int val;
};
struct RPM_for_TCL::RPMVerbosity
{
	const char	*name;
	int		     value;
};

const RPM_for_TCL::Tag_list *RPM_for_TCL::tags = 0;

const char RPM_for_TCL::version[] = FULL_VERSION;
const char RPM_for_TCL::build_date[] = __DATE__" "__TIME__;

const RPM_for_TCL::Cmd_list RPM_for_TCL::cmds[] =
{
 {{"help","Get help on subcommands"},&RPM_for_TCL::Help},
 {{"version","Get version #"},&RPM_for_TCL::Version},
 {{"osversion","Get OS version"},&RPM_for_TCL::OS_ver},
 {{"archversion","Get architecture"},&RPM_for_TCL::Arch_ver},
 {{"closedb","Close database"},&RPM_for_TCL::Close_db},
 {{"package","Find matching packages"},&RPM_for_TCL::FindPackage},
 {{"file","Read RPM file header"},&RPM_for_TCL::File},
 {{"info","Return info on a header"},&RPM_for_TCL::Info},
 {{"tags","Return tags in a header, or all tags known (if no header given)"},&RPM_for_TCL::Tags},
 {{"type","Return data type of tag"},&RPM_for_TCL::TagType},
 {{"transaction","create a transaction list"},&RPM_for_TCL::Create_transaction},
 {{"increaseverbosity","increase debugging verbosity"},&RPM_for_TCL::IncreaseVerbosity},
 {{"decreaseverbosity","Decrease debugging verbosity"},&RPM_for_TCL::DecreaseVerbosity},
 {{"verbosity","Set/get debugging verbosity"},&RPM_for_TCL::Verbosity},
 {{"problem","create a problem report"},&RPM_for_TCL::NewProblem},
 {{"macro","modify in-memory RPM rpmrc parameters"},&RPM_for_TCL::Macro},
 {{"expand","expand RPM macro"},&RPM_for_TCL::Expand},
 {{"cache","RPM cache sub-commands"},&RPM_for_TCL::Cache},
 {{"compare","compare 2 RPM header versions"},&RPM_for_TCL::Compare},
 {{0,0},0}
};

const RPM_for_TCL::Cmd_list RPM_for_TCL::Cache_cmds[] =
{
 {{"help","Help on cache sub-commands"},&RPM_for_TCL::Cache_Help},
 {{"add","Add RPMs to the cache"},&RPM_for_TCL::Cache_Add},
 {{"search","Search cache for named RPM"},&RPM_for_TCL::Cache_Search},
 {{"init","Initialize cache data base"},&RPM_for_TCL::Cache_Init},
 {{"remove","Remove RPMs from the cache"},&RPM_for_TCL::Cache_Remove},
 {{"rebuild","Rebuild cache database"},&RPM_for_TCL::Cache_Rebuild},
  {{0,0},0}
};

const RPM_for_TCL::Tag_list RPM_for_TCL::extra_tags[] = 
{
 {"RPMTAG_BADSHA1_2",RPMTAG_BADSHA1_2},
 {"",},
 {0,0}
};

RPM_for_TCL::RPMVerbosity RPM_for_TCL::verbosities[] =
{
	{ "alert",    RPMLOG_ALERT },
	{ "critical", RPMLOG_CRIT },
	{ "debug",    RPMLOG_DEBUG },
	{ "emerg",    RPMLOG_EMERG },
	{ "error",    RPMLOG_ERR },
	{ "info",     RPMLOG_INFO },
	{ "notice",   RPMLOG_NOTICE },
	{ "warning",  RPMLOG_WARNING },
	{ 0, 0 }
};

static char *rpmtag_to_tcl(const char *tagname)
{
   // Clip the RPMTAG_ off the front of the tag name, prepend a "-", and lowercase it,
   // to make it more TCL-ish
   const char *base = tagname + sizeof("RPMTAG")-1; // Skip over everything but the "_"
   int len = strlen(base);
   char *name = new char[len+1];
   if (!name)
      return 0;
   for (int j = 0; j < len; ++j)
   {
      name[j] = tolower(base[j]);
   }
   name[len] = 0; // Force termination
   name[0] = '-'; // Prepend the dash
   return name;
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::DBs_are_dirty                           *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Note that "somebody, somewhere" has dirtied one or   *
*                   more database                                        *
*************************************************************************/
int RPM_for_TCL::db_dirtied_count = 0;
void RPM_for_TCL::DBs_are_dirty(void)
{
   ++db_dirtied_count;
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Are_DBs_dirty                           *
* ARGUMENTS     :   caller's "dirty count"                               *
* RETURNS       :   1 if data bases have been updated, else 0            *
*                   also updates dirty counter to current value          *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   See if "somebody" has updated the databases          *
*************************************************************************/
int RPM_for_TCL::Are_DBs_dirty(int &my_count)
{
   int my = my_count;
   my_count = db_dirtied_count;
   return (db_dirtied_count != my);
}
/*************************************************************************
* FUNCTION      :   Rpm_Init                                             *
* ARGUMENTS     :   interp                                               *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create the RPM command                               *
*************************************************************************/
int Rpm_Init(Tcl_Interp *interp)
{
   // If needed, hook up the RPM tag list
   if (!RPM_for_TCL::tags)
   {
      int extra_tag_count = (sizeof(RPM_for_TCL::extra_tags)/sizeof(RPM_for_TCL::extra_tags[0]));
      RPM_for_TCL::Tag_list *tags = new RPM_for_TCL::Tag_list[rpmTagTableSize+extra_tag_count];
      if (!tags)
      {
         return RPM_for_TCL::Error(interp, "Cannot allocate memory for RPM tag list");
      }
      int i = 0;
      for (; i < rpmTagTableSize; ++i)
      {
         tags[i].val  = rpmTagTable[i].val;
         tags[i].name = rpmtag_to_tcl(rpmTagTable[i].name);
         if (!tags[i].name)
            return RPM_for_TCL::Error(interp, "Cannot allocate memory for RPM tag list");
      }
      // Now append the extra tags
      for (int j = 0; RPM_for_TCL::extra_tags[j].name; ++j,++i)
      {
         tags[i].val = RPM_for_TCL::extra_tags[j].val;
         tags[i].name = rpmtag_to_tcl(RPM_for_TCL::extra_tags[j].name);
         if (!tags[i].name)
            return RPM_for_TCL::Error(interp, "Cannot allocate memory for RPM tag list");
      }
      tags[i].name = 0;
      RPM_for_TCL::tags = tags;
   }
   if (RPM_for_TCL::Cmd_exists(interp,"RPM"))
      return RPM_for_TCL::Error(interp, "\"RPM\" command already exists");

   RPM_for_TCL *ptr = new RPM_for_TCL(interp);
   if (!ptr)
   {
      return RPM_for_TCL::Error(interp,"No memory to create object");
      return TCL_ERROR;
   }
   return Tcl_Eval(interp,"::RPM::Init");
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::RPM_for_TCL                             *
* ARGUMENTS     :   interp                                               *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create the RPM command object                        *
*************************************************************************/
RPM_for_TCL::RPM_for_TCL(Tcl_Interp *interp)
            :Cmd_base(interp,"RPM",&cmds[0].base,sizeof(cmds[0])),
             database(0),
             cache_db(0),
             transaction_counter(0),
             my_dirty_count(0)
{
   Tcl_PkgProvide(interp, "RPM", version);
   
   // read the RPM config files
   rpmReadConfigFiles(0,0);
   // set my dirty counter
   Are_DBs_dirty(my_dirty_count);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::~RPM_for_TCL                            *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Destroy the RPM command object                       *
*************************************************************************/
RPM_for_TCL::~RPM_for_TCL()
{
   Close_db();
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Exec                                    *
* ARGUMENTS     :   which cmd to do,interp, arg count, arg list          *
* RETURNS       :   per subcommands                                      *
* EXCEPTIONS    :   Per subcommands                                      *
* PURPOSE       :   Carry out the RPM command                            *
*************************************************************************/
int RPM_for_TCL::Exec(int which,Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   // Before doing any command, see if the databases are dirty - if so, close them
   if (Are_DBs_dirty(my_dirty_count))
      Close_db();
  
   return (this->*cmds[which].func)(interp,objc,objv);
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Help                                    *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   TCL_OK                                               *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Return help text                                     *
*************************************************************************/
int RPM_for_TCL::Help(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp,"<problem obj>"," - ","manipulate a problem report object\n",0);
    Tcl_AppendResult(interp,"<header obj>"," - ","manipulate an RPM header object\n",0);
    return Cmd_base::Help(interp,objc,objv);
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Cmd_hook                                *
* ARGUMENTS     :   interp, args from TCL                                *
* RETURNS       :   TCL_RETURN if we did not handle the command          *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Command hook before standard processing              *
*************************************************************************/
int RPM_for_TCL::Cmd_hook(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   // if the first parm is a list, get the first element
   Tcl_Obj *first = objv[1];
   while (first->typePtr == Cmd_base::listtype)
   {
      Tcl_Obj *element = 0;
	  if (Tcl_ListObjIndex(interp,first,0,&element) != TCL_OK)
	  {
	     return TCL_ERROR;
	  }
	  first = element;
   }
   
   if (first->typePtr == &RPMPRoblem_Obj::mytype)
   {
      RPMPRoblem_Obj *dis = (RPMPRoblem_Obj *)(first->internalRep.otherValuePtr);
      return dis->Problem(interp,objc,objv,objv[1],2);
   }
   if (first->typePtr == &RPMHeader_Obj::mytype)
      return HeaderCmd(interp,objc,objv);
   return TCL_RETURN; // Let normal processing happen
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Open_db                                 *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   open database                                        *
*************************************************************************/
int RPM_for_TCL::Open_db(void)
{
   if (!database)
   {
      if (rpmdbOpen(0,&database,O_RDWR,DB_CREATE) != 0)
      {
         if (rpmdbOpen(0,&database,O_RDONLY,DB_RDONLY) != 0)
         {
            return Error("Couldn't open RPM database: %s",rpmErrorString());
         }
      }
   }
   if (!cache_db)
   {
      char *result= rpmExpand("%{_solve_dbpath}",0);
      if (result)
      {
         if (rpmdbOpen(result,&cache_db,O_RDWR,DB_CREATE) != 0)
         {
            return Error("Couldn't open RPM cache database: %s",rpmErrorString());
         }
         free(result);
      }
   }
   return TCL_OK;
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Close_db                                *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   close any open database                              *
*************************************************************************/
void RPM_for_TCL::Close_db(void)
{
   if (database)
   {
      rpmdbClose(database);
   }
   database = 0;
   if (cache_db)
   {
      rpmdbSync(cache_db);
      rpmdbClose(cache_db);
   }
   cache_db = 0;
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Version                                 *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Return version string                                *
*************************************************************************/
int RPM_for_TCL::Version(Tcl_Interp *interp,int , Tcl_Obj *const [])
{
   return OK("%s %s (built %s)\nPlease report bugs to %s",PACKAGE_NAME,version,build_date,PACKAGE_BUGREPORT);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::OS_ver                                  *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Return OS version and number                         *
*************************************************************************/
int RPM_for_TCL::OS_ver(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,"requires no argument");
      return TCL_ERROR;
   }
   
   const char *name = 0;
   int rev = 0;
   rpmGetOsInfo(&name,&rev);
   
   Tcl_Obj *result = Tcl_NewStringObj(name,-1);
   Tcl_IncrRefCount(result);
   Tcl_ListObjAppendElement(interp,result,Tcl_NewLongObj(rev));
   return OK(result);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Arch_ver                                *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Return arch                                          *
*************************************************************************/
int RPM_for_TCL::Arch_ver(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,"requires no argument");
      return TCL_ERROR;
   }
   
   const char *name = 0;
   int rev = 0;
   rpmGetArchInfo(&name,&rev);
   
   Tcl_Obj *result = Tcl_NewStringObj(name,-1);
   Tcl_IncrRefCount(result);
   Tcl_ListObjAppendElement(interp,result,Tcl_NewLongObj(rev));
   return OK(result);
}


/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Close_db                                *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Close any open database                              *
*************************************************************************/
int RPM_for_TCL::Close_db(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,"requires no argument");
      return TCL_ERROR;
   }
   Close_db();
   return TCL_OK;
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::FindPackage                             *
* ARGUMENTS     :   name of item to search for, tag for name             *
*                   interpreter (for errors), database to search         *
* RETURNS       :   Tcl_Obj of results or 0 if not found                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given a name and tag, return database indexes of all *
*                   matching packages                                    *
*************************************************************************/
int RPM_for_TCL::FindPackage_core(Tcl_Obj *&result,Tcl_Obj *name,rpmTag tag,rpmdb database,Tcl_Interp *_interp)
{
    char *name_string = 0;
    int   name_len    = 0;
    if (name)
    {
    	// is it already a package header?
        if (name->typePtr == &RPMHeader_Obj::mytype)
        {
           Tcl_IncrRefCount(name);
           result = name;
           return TCL_OK;
        }
	    // We may have been given a list (of list of lists..) - check for that
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
	          if (FindPackage_core(result,element,tag,database,_interp) != TCL_OK)
	             return TCL_ERROR;
	       }
	       return TCL_OK;
	    }
        // OK, get the string rep and build the iterator
        name_string = Tcl_GetStringFromObj(name,&name_len);
	}
	rpmdbMatchIterator matches = rpmdbInitIterator(database,tag,name_string,name_len);
    // Did we find any matches?
    if (!matches)
    {
        return TCL_OK;
    }
    // OK, so append the list of DB entries into the return value
    Header hdr = rpmdbNextIterator(matches);
    if (!hdr)
    {
        // No matches - then err
        rpmdbFreeIterator(matches);
        return TCL_OK;
    }
    while (hdr)
    {
        int db_entry = rpmdbGetIteratorOffset(matches);
        Tcl_Obj *list = RPMHeader_Obj::Create_from_header(hdr,0,db_entry);
        if (!list)
           break;
        result = Grow_list(result,list,_interp);
        hdr    = rpmdbNextIterator(matches);
    }
    rpmdbFreeIterator(matches);
    return TCL_OK;
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::FindPackage                             *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given a package name, return database indexes of all *
*                   matching packages                                    *
*************************************************************************/
int RPM_for_TCL::FindPackage(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (Open_db() != TCL_OK)
      return TCL_ERROR;
   return FindPackage_cmd(database,2,interp,objc,objv);
}
int RPM_for_TCL::FindPackage_cmd(rpmdb db,int first_arg,Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{      
   Tcl_Obj *args = 0;
   rpmTag tag    = RPMTAG_NAME;
   
   // any interesting things, like tags, on the command line?
   while (objc > first_arg)
   {
      int which = 0;
      if (Tcl_GetIndexFromObjStruct(0,objv[first_arg],(char **)&tags[0].name,sizeof(tags[0]),
                                 "tag",0,&which
                                ) != TCL_OK)
      
      {
         // Not a tag, so we are done
         break;
      }
      // OK, tag name found - set it and consume it
      tag = (rpmTag)tags[which].val;
      ++objv;
      --objc;
   }
   // Build a list of indexes matching the packages given.
   if (objc > first_arg)
   {
      args = Tcl_NewListObj(objc-first_arg,objv+first_arg);
      Tcl_IncrRefCount(args);
   }
   Tcl_Obj *result = 0;
   int rc = FindPackage_core(result,args,tag,db,interp);
   if (args)
      Tcl_DecrRefCount(args); // OK, release the working list
   return Return(rc,result);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::File_core                               *
* ARGUMENTS     :   item to add to, item to check                        *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given a file name, return package header             *
*************************************************************************/
int RPM_for_TCL::File_core(Tcl_Obj *&result,Tcl_Obj *name)
{
    // We may have been given a list (of list of lists..) - check for that
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
          if (File_core(result,element) != TCL_OK)
          {
             return TCL_ERROR;
          }
       }
       return TCL_OK;
    }
    // Not a list - so it is either a file name or a package.
    if (name->typePtr == &RPMHeader_Obj::mytype)
    {
       Tcl_IncrRefCount(name);
       result = name;
       return TCL_OK;
    }
        
    // OK, find the file.

    const char *fname = Tcl_FSGetNativePath(name);
    Header header;
    if (!fname)
       return TCL_ERROR;
    // So good of RPM to introduce it's own frickn file abstraction...
    FD_t rpm_fd = Fopen(fname, "r");
    if (!rpm_fd)
    {
       return Error("Error opening %s: %s",fname,rpmErrorString());
    }
    rpmts transaction = rpmtsCreate();
    assert(transaction);
    int_32 tid = rpmtsGetTid(transaction);

    int rpm_result = rpmReadPackageFile(transaction, rpm_fd, fname, &header);
    Fclose(rpm_fd);
    rpmtsFree(transaction);
    switch (rpm_result) 
    {
        case RPMRC_NOTTRUSTED:
        return Error("Untrusted RPM");
        
        case RPMRC_NOKEY:
        {
            // Do I allow RPMs with unknown keys to be processed?
            Tcl_Obj *config = Tcl_NewStringObj("::RPM::Config",-1);
            Tcl_Obj *part   = Tcl_NewStringObj("unknown_keys",-1);
            Tcl_Obj *allow_unknown = Tcl_ObjGetVar2(_interp, config, part, TCL_GLOBAL_ONLY);
            Tcl_DecrRefCount(config);
            Tcl_DecrRefCount(part);
            int value = 0; // By default, do not allow unknown keys
            if (allow_unknown)
            {
                Tcl_GetIntFromObj(0,allow_unknown,&value);
            }
            if (!value)
               return Error("Key not found");
        }
        break;
        
        case RPMRC_OK:
        break;
            
        default:
           printf("err\n");
        return Error("Error reading %s: %s",fname,rpmErrorString());
    }
    // Save the info on the object
    result = Grow_list(result,RPMHeader_Obj::Create_from_header(header,fname,-1,tid));
    return TCL_OK;
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::File                                    *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given a file name, return package header             *
*************************************************************************/
int RPM_for_TCL::File(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   // If no filename, barf
   if (objc == 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,"file [file...]");
      return TCL_ERROR;
   }
   
   // OK, so we will build a list of files matching the name(s) given.
   Tcl_Obj *result = 0;

      // Build a list of indexes matching the packages given.
   Tcl_Obj *args = Tcl_NewListObj(objc-2,objv+2);
   if (!args)
      return Error("Cannot concat arglist!");
   Tcl_IncrRefCount(args);

   int rc = File_core(result,args);
   Tcl_DecrRefCount(args);
   
   return Return(rc,result);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Info                                    *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given a header, return the info on it                *
*************************************************************************/
int RPM_for_TCL::Info(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if ((objc < 3) || (objc > 4))
   {
      Tcl_WrongNumArgs(interp,2,objv,"<package> [-tagname]");
      return TCL_ERROR;
   }
   // OK, did they spec a tagname?
   if (objc == 4)
   {
     return Header_core(interp,objv[2],objv[3]);
   }
   return Header_core(interp,objv[2],0);
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::HeaderCmd                               *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given a header, return the info on it                *
*************************************************************************/
int RPM_for_TCL::HeaderCmd(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if ((objc < 2) || (objc > 3))
   {
      Tcl_WrongNumArgs(interp,2,objv,"[-tagname]");
      return TCL_ERROR;
   }
   // OK, did they spec a tagname?
   if (objc == 3)
   {
     return Header_core(interp,objv[1],objv[2]);
   }
   return Header_core(interp,objv[1],0);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Header_core                             *
* ARGUMENTS     :   interp, header, tag                                  *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given a header, return the info on it                *
*************************************************************************/
int RPM_for_TCL::Header_core(Tcl_Interp *interp,Tcl_Obj *hdr,Tcl_Obj *tag)
{
   if (Tcl_ConvertToType(interp,hdr,&RPMHeader_Obj::mytype) != TCL_OK)
      return TCL_ERROR;
   RPMHeader_Obj *header = ( RPMHeader_Obj *)(hdr->internalRep.otherValuePtr);
   
   // OK, did they spec a tagname?
   if (tag)
   {
     // Try to find the tag name
     int which = 0;
     if (Tcl_GetIndexFromObjStruct(interp,tag,(char **)&tags[0].name,sizeof(tags[0]),
                                 "tag",0,&which
                                ) != TCL_OK)
        return TCL_ERROR;
     Tcl_Obj *list = header->HeaderEntry(tags[which].val);
     return OK(list);
   }
   // No tag given, so they get EVERYTHING
   Tcl_Obj *list = Tcl_NewObj();
   Tcl_IncrRefCount(list);
   HeaderIterator iterator = headerInitIterator(*header);
   for (;;)
   {
      // Get next item in header
      int tag = 0;
      Tcl_Obj *item = header->HeaderNextIterator(iterator,tag);

      if (!item) // End of list? then bail
         break;

      // we need to supply the tag name
      Tcl_Obj *tagname = 0;
      for (int j = 0; j < rpmTagTableSize; ++j)
      {
         if (rpmTagTable[j].val == tag)
         {
            tagname = Tcl_NewStringObj(tags[j].name,-1);
            break;
         }
      }

      if (!tagname)
      {
         char temp[100];
         snprintf(temp,sizeof(temp),"<unknown tag %d>",tag);
         tagname = Tcl_NewStringObj(temp,-1);
      }
      Tcl_ListObjAppendElement(interp,list,tagname);
      Tcl_ListObjAppendElement(interp,list,item);
      Tcl_DecrRefCount(item);
   }
   // Add db or filename info
   if (header->Filename())
   {
         Tcl_ListObjAppendElement(interp,list,Tcl_NewStringObj("-filename",-1));
         Tcl_ListObjAppendElement(interp,list,Tcl_NewStringObj(header->Filename(),-1));
   }
   if (header->DB_entry() > -1)
   {
      Tcl_ListObjAppendElement(interp,list,Tcl_NewStringObj("-db_entry",-1));
      Tcl_ListObjAppendElement(interp,list,Tcl_NewLongObj(header->DB_entry()));
   }
   headerFreeIterator(iterator);
   return OK(list);
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Tags                                    *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   return all known tags, or all tags in a header       *
*************************************************************************/
int RPM_for_TCL::Tags(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   // bad args?
   if (objc > 3)
   {
      Tcl_WrongNumArgs(interp,2,objv,"[header]");
      return TCL_ERROR;
   }
   
   // If no package, list ALL tags
   if (objc == 2)
   {
      Tcl_Obj *list = Tcl_NewObj();
      Tcl_IncrRefCount(list);
      for (int j = 0; j < rpmTagTableSize; ++j)
      {
         Tcl_ListObjAppendElement(interp,list,Tcl_NewStringObj(tags[j].name,-1));
      }
      return OK(list);
   }
   else
   {
      // List all tags in header
      Tcl_Obj *hdr = objv[2];
      if (Tcl_ConvertToType(interp,hdr,&RPMHeader_Obj::mytype) != TCL_OK)
         return TCL_ERROR;

      Tcl_Obj *list = Tcl_NewObj();
      Tcl_IncrRefCount(list);
      RPMHeader_Obj *header = ( RPMHeader_Obj *)(hdr->internalRep.otherValuePtr);
      HeaderIterator iterator = headerInitIterator(*header);
      for (;;)
	  {
	      // Get next item in header
	      int tag = 0;
	      Tcl_Obj *item = header->HeaderNextIterator(iterator,tag);
	      if (!item) // End of list? then bail
	         break;
	      Tcl_DecrRefCount(item); // We don't need the item so returned.
	      
	      Tcl_Obj *tagname = 0;
	      for (int j = 0; j < rpmTagTableSize; ++j)
	      {
	         if (rpmTagTable[j].val == tag)
	         {
	            tagname = Tcl_NewStringObj(tags[j].name,-1);
	            break;
	         }
	      }
	
	      if (!tagname)
	      {
	         char temp[100];
	         snprintf(temp,sizeof(temp),"<unknown tag %d>",tag);
	         tagname = Tcl_NewStringObj(temp,-1);
	      }
	      Tcl_ListObjAppendElement(interp,list,tagname);
	  }
      return OK(list);
   }
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::TagType                                 *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given a header and tag, return data type             *
*************************************************************************/
int RPM_for_TCL::TagType(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   // If no object, barf
   if (objc != 4)
   {
      Tcl_WrongNumArgs(interp,2,objv,"header <-tagname>");
      return TCL_ERROR;
   }
   Tcl_Obj *hdr = objv[2];
   if (Tcl_ConvertToType(interp,hdr,&RPMHeader_Obj::mytype) != TCL_OK)
      return TCL_ERROR;
   RPMHeader_Obj *header = ( RPMHeader_Obj *)(hdr->internalRep.otherValuePtr);
   int which = 0;
   if (Tcl_GetIndexFromObjStruct(interp,objv[3],(char **)&tags[0].name,sizeof(tags[0]),
                                 "tag",0,&which
                                ) != TCL_OK)
        return TCL_ERROR;
   return OK(header->TagType(tags[which].val));
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::IncreaseVerbosity                       *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Increase the RPM verbosity                           *
*************************************************************************/
int RPM_for_TCL::IncreaseVerbosity(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,"takes no parameters");
      return TCL_ERROR;
   }
   rpmIncreaseVerbosity();
   return Verbosity(interp,objc,objv);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::DecreaseVerbosity                       *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Increase the RPM verbosity                           *
*************************************************************************/
int RPM_for_TCL::DecreaseVerbosity(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 2)
   {
      Tcl_WrongNumArgs(interp,2,objv,"takes no parameters");
      return TCL_ERROR;
   }
   rpmDecreaseVerbosity();
   return Verbosity(interp,objc,objv);
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::DecreaseVerbosity                       *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Increase the RPM verbosity                           *
*************************************************************************/
int RPM_for_TCL::Verbosity(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc > 3)
   {
      Tcl_WrongNumArgs(interp,2,objv,"[level]");
      return TCL_ERROR;
   }
   if (objc == 3)
   {
      int which = 0;
      if (Tcl_GetIndexFromObjStruct(interp,objv[2],(char **)&verbosities[0].name,sizeof(verbosities[0]),
                                 "verbosity",0,&which
                                ) != TCL_OK)
         return TCL_ERROR;
      rpmSetVerbosity(verbosities[which].value);
   }
   // Get current verbosity
   int verbosity = rpmlogSetMask(0);
   const char *msg = 0;
   int old_verbosity = 0;
   // Now, find the highest named level < the current level
   for (int i = 0; verbosities[i].name; ++i)
   {
      int this_verbosity = RPMLOG_MASK(verbosities[i].value);
      if (!msg)
      {
         msg           = verbosities[i].name;
         old_verbosity = this_verbosity;
      }
      else if (this_verbosity < old_verbosity)
      {
         //lower, so igore it
      }
      else if (verbosity >= this_verbosity)
      {
         msg           = verbosities[i].name;
         old_verbosity = this_verbosity;
      }
   }

   return OK(msg);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Macro                                   *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Define an RPM macro (rpmrc -type entry)              *
*************************************************************************/
int RPM_for_TCL::Macro(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   // Lazy cuss that I am, I'm going to just lump all args into one big list.
   Tcl_Obj *args = Tcl_NewListObj(objc-2,objv+2);
   if (!args)
      return Error("Cannot concat arglist!");
      
   Tcl_IncrRefCount(args);
   int len = 0;
   char *arg = Tcl_GetStringFromObj(args,&len);
   int rc = rpmDefineMacro(0,arg,0);
   Tcl_DecrRefCount(args);
   // Don't know what the return codes are now - looks like it is always 0.
   // Just chuck it back to TCL
   return OK(rc);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Expand                                  *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Expand RPM macros (rpmrc entries)                    *
*************************************************************************/
int RPM_for_TCL::Expand(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   // Lazy cuss that I am, I'm going to just lump all args into one big list.
   Tcl_Obj *args = Tcl_NewListObj(objc-2,objv+2);
   if (!args)
      return Error("Cannot concat arglist!");
      
   Tcl_IncrRefCount(args);
   int len = 0;
   char *arg = Tcl_GetStringFromObj(args,&len);
   char *result= rpmExpand(arg,0);
   Tcl_DecrRefCount(args);
   if (!result)
      return TCL_OK;
      
   args = Tcl_NewStringObj(result,-1);
   Tcl_IncrRefCount(args);
   free(result);
   return OK(args);
}


/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Cache                                   *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   add one or more files to the cache database          *
*************************************************************************/
int RPM_for_TCL::Cache(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   int which = 0;
   int rv = TCL_OK;
   
   if (objc < 3)
   {
      Help(interp,objc,objv);
      return TCL_ERROR;
   }
   // Let the derived class have a crack at it
   rv = Cmd_hook(interp,objc,objv);
   if (rv != TCL_RETURN)
      return rv;
      
   if (Tcl_GetIndexFromObjStruct(interp,objv[2],(char **)&Cache_cmds[0].base.cmd,sizeof(Cache_cmds[0]),
                                 "subcommand",0,&which
                                ) != TCL_OK)
      return TCL_ERROR;

   return (this->*Cache_cmds[which].func)(interp,objc,objv);
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Cache_Help                              *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   help for cache sub-dirs                              *
*************************************************************************/
int RPM_for_TCL::Cache_Help(Tcl_Interp *interp,int , Tcl_Obj *const [])
{
   for (int i = 0; Cache_cmds[i].base.cmd != 0; ++i)
   {
      Tcl_AppendResult(interp,Cache_cmds[i].base.cmd," - ",Cache_cmds[i].base.help,"\n",0);
   }
   return TCL_OK;
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Cache_Add                               *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   add one or more files to the cache database          *
*************************************************************************/
int RPM_for_TCL::Cache_Add(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc < 4)
   {
      Tcl_WrongNumArgs(interp,3,objv,0);
      return TCL_ERROR;
   }
   // If needed, open the database
   Open_db();
      
   // Take all args on the command line, and make one big list
   Tcl_Obj *args = Tcl_NewObj();
   Tcl_IncrRefCount(args);
   
   for (int i = 3; i < objc; ++i)
   {
      if (Tcl_ListObjAppendList(interp,args,objv[i]) != TCL_OK)
      {
         Tcl_DecrRefCount(args);
         return TCL_ERROR;
      }
   } 
   
   // Iterate through the list, attempting to read each item.
   int count = 0;
   if (Tcl_ListObjLength(interp, args, &count) != TCL_OK)
   {
       goto error;
   }
   for (int i = 0; i < count; ++i)
   {
      Tcl_Obj *item = 0;
      if (Tcl_ListObjIndex(interp, args, i, &item) != TCL_OK)
         goto error;
      
      // Try to make into a header object
      if (Tcl_ConvertToType(interp,item,&RPMHeader_Obj::mytype) != TCL_OK)
         goto error;
      RPMHeader_Obj *header = ( RPMHeader_Obj *)(item->internalRep.otherValuePtr);
      // Get the overall MD5 sum, and use that as an index into the cache database
      Tcl_Obj *md5 = header->HeaderEntry(RPMTAG_SIGMD5);
      int md5_size = 0;
      char *md5_bytes = Tcl_GetStringFromObj(md5,&md5_size);
      // Locate that header in the solution database
      rpmdbMatchIterator matches = rpmdbInitIterator(cache_db,RPMTAG_SIGMD5,md5_bytes,md5_size);
      int count = rpmdbGetIteratorCount(matches); // How many matches?
      rpmdbFreeIterator(matches);
      Tcl_DecrRefCount(md5);
      if (!count) // New item - add to data base
      {
         int_32 tid = header->Get_transaction_time();
         const char *name = header->Filename();
         struct stat finfo;
         if (stat(name,&finfo) != 0)
         {
            Error("Error reading file info: %s",strerror(errno));
            goto error;
         }
         if (headerAddOrAppendEntry(*header, RPMTAG_CACHECTIME,RPM_INT32_TYPE, &tid, 1) != 1)
         {
            Error("Error adding cache tid: %s",rpmErrorString());
            goto error;
         }
         if (headerAddOrAppendEntry(*header, RPMTAG_CACHEPKGPATH,RPM_STRING_ARRAY_TYPE,&name, 1) != 1)
         {
            Error("Error adding cache path: %s",rpmErrorString());
            goto error;
         }
         if (headerAddOrAppendEntry(*header, RPMTAG_CACHEPKGSIZE,RPM_INT32_TYPE, &finfo.st_size, 1) != 1)
         {
            Error("Error adding cache size: %s",rpmErrorString());
            goto error;
         }
         if (headerAddOrAppendEntry(*header, RPMTAG_CACHEPKGMTIME,RPM_INT32_TYPE, &finfo.st_mtime, 1) != 1)
         {
            Error("Error adding cache time: %s",rpmErrorString());
            goto error;
         }
 
         /* --- Add new cache header to database. */
         if (rpmdbAdd(cache_db, tid, *header, NULL, NULL) != 0)
         {
            Error("Error adding package: %s",rpmErrorString());
            goto error;
         }
      }
   }

   Tcl_DecrRefCount(args);
   return TCL_OK;
   
error:
   Tcl_DecrRefCount(args);
   return TCL_ERROR;
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Cache_Search                            *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Given a package name, return database indexes of all *
*                   matching packages                                    *
*************************************************************************/
int RPM_for_TCL::Cache_Search(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (Open_db() != TCL_OK)
      return TCL_ERROR;
   return FindPackage_cmd(cache_db,3,interp,objc,objv);
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Cache_Init                              *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Init the RPM cache db                                *
*************************************************************************/
int RPM_for_TCL::Cache_Init(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 3)
   {
      Tcl_WrongNumArgs(interp,3,objv,"takes no parameters");
      return TCL_ERROR;
   }
   Close_db();
   char *result= rpmExpand("%{_solve_dbpath}",0);
   if (result)
   {
      if (rpmdbInit(result,O_RDWR) != 0)
      {
         return Error("Error initializing cache database %s: %s",result,rpmErrorString());
      }
   }
   return TCL_OK;
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Cache_Remove                            *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   remove one or more files from the cache database     *
*************************************************************************/
int RPM_for_TCL::Cache_Remove(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc < 4)
   {
      Tcl_WrongNumArgs(interp,3,objv,0);
      return TCL_ERROR;
   }
   // If needed, open the database
   Open_db();
      
   // Take all args on the command line, and make one big list
   Tcl_Obj *args = Tcl_NewObj();
   Tcl_IncrRefCount(args);
   
   for (int i = 3; i < objc; ++i)
   {
      if (Tcl_ListObjAppendList(interp,args,objv[i]) != TCL_OK)
      {
         Tcl_DecrRefCount(args);
         return TCL_ERROR;
      }
   } 
   
   // Iterate through the list, attempting to remove each item.
   int count = 0;
   if (Tcl_ListObjLength(interp, args, &count) != TCL_OK)
   {
       goto error;
   }
   for (int i = 0; i < count; ++i)
   {
      Tcl_Obj *item = 0;
      if (Tcl_ListObjIndex(interp, args, i, &item) != TCL_OK)
         goto error;
      
      // Try to make into a header object
      if (Tcl_ConvertToType(interp,item,&RPMHeader_Obj::mytype) != TCL_OK)
         continue;
      RPMHeader_Obj *header = ( RPMHeader_Obj *)(item->internalRep.otherValuePtr);

            // Get the overall MD5 sum, and use that as an index into the cache database
      Tcl_Obj *md5 = header->HeaderEntry(RPMTAG_SIGMD5);
      int md5_size = 0;
      char *md5_bytes = Tcl_GetStringFromObj(md5,&md5_size);
      // Locate that header in the solution database
      rpmdbMatchIterator matches = rpmdbInitIterator(cache_db,RPMTAG_SIGMD5,md5_bytes,md5_size);
      while (rpmdbNextIterator(matches))
      {
         int hdrnum =  rpmdbGetIteratorOffset(matches); // Get db entry #
         if (rpmdbRemove(cache_db,0,hdrnum,0,0) != 0)
         {
            Error("Error freeing %s: %s\n",Tcl_GetStringFromObj(item,0),rpmErrorString());
            rpmdbFreeIterator(matches);
            goto error;
         }
      }
      rpmdbFreeIterator(matches);
   }

   Tcl_DecrRefCount(args);
   return TCL_OK;
   
error:
   Tcl_DecrRefCount(args);
   return TCL_ERROR;
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Cache_Rebuild                           *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Rebuild the database indexes                         *
*************************************************************************/
int RPM_for_TCL::Cache_Rebuild(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 3)
   {
      Tcl_WrongNumArgs(interp,3,objv,"Takes no parameters");
      return TCL_ERROR;
   }
   Close_db();
   char *path= rpmExpand("%{_solve_dbpath}",0);
   if (rpmdbRebuild(path,0,0) != 0 )
   {
      return Error("Error rebuilding cache db: %s",rpmErrorString());
   }
   return TCL_OK;
}

/*************************************************************************
* FUNCTION      :   RPM_for_TCL::Compare                                 *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Compare 2 RPM headers based on version               *
*************************************************************************/
int RPM_for_TCL::Compare(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   if (objc != 4)
   {
      Tcl_WrongNumArgs(interp,2,objv,"<header 1> <header 2>");
      return TCL_ERROR;
   }
   Tcl_Obj *o1 = objv[2];
   Tcl_Obj *o2 = objv[3];
   // De-listify any lists
   while (o1->typePtr == Cmd_base::listtype)
   {
       if (Tcl_ListObjIndex(interp,o1,0,&o1) != TCL_OK)
	   {
	      return TCL_ERROR;
	   }
   }
   while (o2->typePtr == Cmd_base::listtype)
   {
       if (Tcl_ListObjIndex(interp,o2,0,&o2) != TCL_OK)
	   {
	      return TCL_ERROR;
	   }
   }
   if (o1->typePtr != &RPMHeader_Obj::mytype)
   {
      return Error("First item is not an RPM header");
   }
   if (o2->typePtr != &RPMHeader_Obj::mytype)
   {
      return Error("Second item is not an RPM header");
   }
   RPMHeader_Obj *h1 = ( RPMHeader_Obj *)(o1->internalRep.otherValuePtr);
   RPMHeader_Obj *h2 = ( RPMHeader_Obj *)(o2->internalRep.otherValuePtr);

   int rv = rpmVersionCompare(*h1,*h2);
   return OK(rv);
}
