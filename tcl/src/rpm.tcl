namespace eval RPM \
{
   namespace export Parse_changelogs
   
   # General namespace configuration variable
   variable Config
   array set Config \
   {
      path {.}
   }
   
   # add one or more directories to the path to search for RPM files
   proc Add_path { args } \
   {
      variable Config
      foreach arg $args \
      {
         if {[llength $arg] > 1} \
         {
            eval Add_path $arg
         } \
         else \
         {
            if [file isdirectory $arg] \
            {
               lappend Config(path) $arg
            } \
            else \
            {
               foreach dir [glob -nocomplain $arg] \
               {
                  if [file isdirectory $dir] \
                  {
                     lappend Config(path) $dir
                  }
               }
            }
         }
      }
   }
   
   # Internal routine - used when converting J. Random TclObj into an RPMHeader.
   # This is done to allow easier customization of this process.
   proc Find_package { pkgname } \
   {
     variable Config

     # First, is this a list of length >1? If so, barf
     if {[llength $pkgname] > 1} \
     {
       return -code error "Cannot convert a list into a header"
     }
     # first, is this a filename? If so, use that
     foreach dir $Config(path) \
     {
         set fn [file join $dir $pkgname]
         if [file exists $fn] \
         {
            set x [RPM file $fn]
            if {$x == ""} { return -code error "bad RPM file $fn" }
            return $x
         }
     }
     # OK, so it is NOT a filename. Is it an already installed package?
     set x [RPM package $pkgname]
     if {$x != ""} \
     {
        return $x
     }
     # Is this in the cache database?
     set x [RPM cache search $pkgname]
     if {$x != ""} \
     {
        return $x
     }
     return -code error "RPM package $pkgname not found in system"
   }
   
   # Given a header, parse the changelogs
   proc Parse_changelogs {package} \
   {
      set date  [RPM info $package -changelogtime]
      set names [RPM info $package -changelogname]
      set lines [RPM info $package -changelogtext]
      
      set max [llength $names]
      lappend results [clock format $date]
      for {set i 0} { $i < $max} { incr i} \
      {
         set name [lindex $names $i]
         set line [lindex $lines $i]
         lappend results [list $name $line]
      }
      return $results
   }
   
   # Callback from install
   proc Callback { bits amount total key} \
   {
      puts "RPM callback: $bits $amount $total $key"
   }
   
   # callback for solving problems
   # 1 - notfound, 0 = skip, -1 = retry
   proc Solve { transaction dependancy } \
   {
      puts "Solve $transaction $dependancy"
      # is this a file that exists?
      if [file exists $dependancy] \
      {
         puts "$dependancy is an existing file - ignoring"
         return 0
      }
      # try to find this dependancy in the cache
      set solve [RPM cache search -providename $dependancy]
      if {$solve != ""} \
      {
         puts "A possible solution is $solve"
         if {[llength $solve] == 1} \
         {
            $transaction add $solve
            return -1
         }
      }
      return 1
   }
   # Set things up at startup - called by the rpm.so module
   proc Init { } \
   {
      variable Config
      if [info exists Config(init_done)] { return }
      set Config(init_done) 1
      # force indexing of the cache path
      set Config(_dbi_tags)     [RPM expand %_dbi_tags]:Cachepkgpath
      RPM macro _dbi_tags $Config(_dbi_tags)
      # Locate where we build the database of available RPMs, and default if needed
      set Config(_solve_dbpath) [RPM expand %_solve_dbpath]
      if {$Config(_solve_dbpath) == "%_solve_dbpath"} \
      {
         set Config(_solve_dbpath) /rpm
         RPM macro _solve_dbpath $Config(_solve_dbpath)
         puts "Defaulting solution cache dir to $Config(_solve_dbpath)"
      }
      set Config(path) $Config(_solve_dbpath)
      # quite all but really severe error messages
      RPM verbosity critical
      
      # Create cache directory, if it does not exist
      if ![file isdirectory $Config(_solve_dbpath)] \
      {
         file mkdir $Config(_solve_dbpath)
         puts "Created cache dir $Config(_solve_dbpath)"
      }
   }

   # Scan a directory, adding all RPMs into the solution database
   proc Scan_callback { type {file ""} {count ""} {total ""} {msg ""}} \
   {
      set percent ""
      if {$total > 0} \
      {
         set percent [format "%3.1f %%" [expr $count*100.0/$total]]
      }
      switch $type \
      {
         DIR   { puts "Scan directory $file"}
         FILE  { puts -nonewline "$percent Examining $file "}
         IN_DB { puts  "already in db"}
         ADD   { puts  "adding to cache"}
         ERR   { puts  "Error $msg"}
         DONE  { puts  "Done"}
      }
      flush stdout
   }
   
   proc Scan { args } \
   {
      variable Config
      set files ""
      # phase 1 - scan all directories and count up all the RPM files
      set dirs $args
      if {$dirs == ""} \
      {
         set dirs $Config(path)
      }
      while {$dirs != "" } \
      {
      	 set this_pass $dirs
      	 set dirs ""
      	 
      	 foreach file $this_pass \
      	 {
      	    if [file isdirectory $file] \
      	    {
      	       Scan_callback DIR $file
      	       set dirs [concat $dirs [glob -nocomplain $file/*]]
      	       continue
      	    }
      	    # accumulate rpm files
      	    if {[file extension $file] == ".rpm"} \
      	    {
      	       lappend files $file
      	    }
      	 }
      }
      # OK, have a list of RPMs. Compute count and start grinding
      set files [lsort $files]
      set total [llength $files]
      set count 0
      
      foreach file $files \
	  {
	     Scan_callback FILE $file $count $total
	     
	     # Seen this filename before?
	     set x [RPM cache search -cachepkgpath $file]
	     if {$x != ""} \
	     {
	        Scan_callback IN_DB $file $count $total 
            incr count
	        continue
	     }
	     Scan_callback ADD $file $count $total
	     if [catch {RPM cache add $file } err ] \
	     {
	     	 Scan_callback ERR $file $count $total $err
	     }
	     incr count
	  }

	  Scan_callback DONE
	  # close the DB to flush to disk
	  RPM close
   }
   
   
   # Given an RPM, locate the newest version in the cache
   proc Find_newest { rpm } \
   {
       set cache [RPM cache search $rpm]
       if {$cache == ""} { return "" }
       # sort the list
       set cache [lsort -decreasing -command {RPM compare} $cache]
       return [lindex $cache 0]
   }
   
   proc Upgrade_callback { type args } \
   {
      switch $type \
      {
        "READ_INSTALLED" { puts "Reading installed packages"}
        "CHECK"          { puts "Looking for newer version of $args"}
        "FOUND"          { puts "Found version $args"}
        "NEWER"          { puts "New version found"}
      }
   }

   # Search the set of all installed RPMs, and return any possible updates
   proc Upgrade { } \
   {
      Upgrade_callback READ_INSTALLED
      set installed [RPM package]
      foreach item $installed \
      {
        Upgrade_callback CHECK [RPM $item -name] [RPM $item -version] [RPM $item -release]
        set x [Find_newest [RPM $item -name]]
        if {$x != ""} \
        {
           Upgrade_callback FOUND [RPM $x -name] [RPM $x -version] [RPM $x -release]
           # is this newer?
           if {[RPM compare $x $item] > 0} \
           {
               Upgrade_callback NEWER
               lappend rv $x
           }
        }
      }
      if [info exists rv] { return $rv}
      return ""
   }
   
   # Search the set of available RPMs, return any newer than installed or not installed.
   proc Find_new { } \
   {
      # get the contents of the archive
      set cache [RPM cache search]
      # break into seperate lists, by name
      foreach item $cache \
      {
         lappend packages([RPM $item -name]) $item
      }
      #Now, sort those lists by version
      set names [array names packages]
      foreach name $names \
      {
          set l [lsort -decreasing -command {RPM compare} $packages($name)]
          set newest [lindex $l 0]
          # see if we have that already
          set installed [RPM package $name]
          if {($installed == "") || ([RPM compare $newest $installed] > 0) } \
          {
             lappend new $newest
          }
      }
      if [info exists new] { return $new}
      return ""
   }
   
   Init
}
