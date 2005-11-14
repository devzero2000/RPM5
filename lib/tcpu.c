#include "system.h"
#include <rpmlib.h>
#include <rpmds.h>
#include "debug.h"

/*
=================
 * processor	: 0
 * vendor_id	: GenuineIntel
 * cpu family	: 6
 * model		: 8
 * model name	: Pentium III (Coppermine)
 * stepping	: 1
 * cpu MHz		: 664.597
 * cache size	: 256 KB
 * physical id	: 0
 * siblings	: 1
 * fdiv_bug	: no
 * hlt_bug		: no
 * f00f_bug	: no
 * coma_bug	: no
 * fpu		: yes
 * fpu_exception	: yes
 * cpuid level	: 2
 * wp		: yes
 * flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 mmx fxsr sse
 * bogomips	: 1327.10
=================
 * processor	: 0
 * vendor_id	: GenuineIntel
 * cpu family	: 15
 * model		: 2
 * model name	: Intel(R) Pentium(R) 4 CPU 2.40GHz
 * stepping	: 9
 * cpu MHz		: 2394.624
 * cache size	: 512 KB
 * physical id	: 0
 * siblings	: 2
 * core id		: 0
 * cpu cores	: 1
 * fdiv_bug	: no
 * hlt_bug		: no
 * f00f_bug	: no
 * coma_bug	: no
 * fpu		: yes
 * fpu_exception	: yes
 * cpuid level	: 2
 * wp		: yes
 * flags		: fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
 * bogomips	: 4793.72
=================
*/

int main (int argc, const char * argv[])
{
    rpmds P = NULL;
    int rc;
    int xx;

    rc = rpmdsCpuinfo(&P, NULL);

    xx = rpmdsPrint(P, NULL);

    P = rpmdsFree(P);

    return rc;
}
