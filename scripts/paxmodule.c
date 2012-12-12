/*
	paxmodule.c: python module to get/set pax flags on an ELF object
	Copyright (C) 2011  Anthony G. Basile

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Python.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef PTPAX
 #include <gelf.h>
#endif

#ifdef NEED_PAX_DECLS
 #define PT_PAX_FLAGS    0x65041580      /* Indicates PaX flag markings */
 #define PF_PAGEEXEC     (1 << 4)        /* Enable  PAGEEXEC */
 #define PF_NOPAGEEXEC   (1 << 5)        /* Disable PAGEEXEC */
 #define PF_SEGMEXEC     (1 << 6)        /* Enable  SEGMEXEC */
 #define PF_NOSEGMEXEC   (1 << 7)        /* Disable SEGMEXEC */
 #define PF_MPROTECT     (1 << 8)        /* Enable  MPROTECT */
 #define PF_NOMPROTECT   (1 << 9)        /* Disable MPROTECT */
 #define PF_RANDEXEC     (1 << 10)       /* DEPRECATED: Enable  RANDEXEC */
 #define PF_NORANDEXEC   (1 << 11)       /* DEPRECATED: Disable RANDEXEC */
 #define PF_EMUTRAMP     (1 << 12)       /* Enable  EMUTRAMP */
 #define PF_NOEMUTRAMP   (1 << 13)       /* Disable EMUTRAMP */
 #define PF_RANDMMAP     (1 << 14)       /* Enable  RANDMMAP */
 #define PF_NORANDMMAP   (1 << 15)       /* Disable RANDMMAP */
#endif

#ifdef XTPAX
 #include <attr/xattr.h>
 #define PAX_NAMESPACE	"user.pax.flags"
#endif

#define FLAGS_SIZE	6


static PyObject * pax_getflags(PyObject *, PyObject *);
static PyObject * pax_setbinflags(PyObject *, PyObject *);
static PyObject * pax_setstrflags(PyObject *, PyObject *);

static PyMethodDef PaxMethods[] = {
	{"getflags",  pax_getflags, METH_VARARGS, "Get the pax flags as a string."},
	{"setbinflags",  pax_setbinflags, METH_VARARGS, "Set the pax flags using binary."},
	{"setstrflags",  pax_setstrflags, METH_VARARGS, "Set the pax flags using string."},
	{NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "pax",						/* m_name */
        "Module for setting PT_PAX and XT_PAX flags",	/* m_doc */
        -1,						/* m_size */
        PaxMethods,					/* m_methods */
        NULL,						/* m_reload */
        NULL,						/* m_traverse */
        NULL,						/* m_clear */
        NULL,						/* m_free */
    };
#endif

static PyObject *PaxError;

PyMODINIT_FUNC
#if PY_MAJOR_VERSION >= 3
PyInit_pax(void)
#else
initpax(void)
#endif
{
	PyObject *m;

#if PY_MAJOR_VERSION >= 3
	m = PyModule_Create(&moduledef);
#else
	m = Py_InitModule("pax", PaxMethods);
#endif

	if (m == NULL)
		return;

	PaxError = PyErr_NewException("pax.error", NULL, NULL);
	Py_INCREF(PaxError);
	PyModule_AddObject(m, "error", PaxError);

#if PY_MAJOR_VERSION >= 3
	return m;
#else
	return;
#endif
}


#ifdef PTPAX
uint16_t
get_pt_flags(int fd)
{
	Elf *elf;
	GElf_Phdr phdr;
	size_t i, phnum;

	uint16_t pt_flags = UINT16_MAX;

	if(elf_version(EV_CURRENT) == EV_NONE)
	{
		PyErr_SetString(PaxError, "get_pt_flags: library out of date");
		return pt_flags;
	}

	if((elf = elf_begin(fd, ELF_C_READ_MMAP, NULL)) == NULL)
	{
		PyErr_SetString(PaxError, "get_pt_flags: elf_begin() failed");
		return pt_flags;
	}

	if(elf_kind(elf) != ELF_K_ELF)
	{
		elf_end(elf);
		PyErr_SetString(PaxError, "get_pt_flags: elf_kind() failed: this is not an elf file.");
		return pt_flags;
	}

	elf_getphdrnum(elf, &phnum);

	for(i=0; i<phnum; i++)
	{
		if(gelf_getphdr(elf, i, &phdr) != &phdr)
		{
			PyErr_SetString(PaxError, "get_pt_flags: gelf_getphdr() failed: could not get phdr.");
			return pt_flags;
		}

		if(phdr.p_type == PT_PAX_FLAGS)
			pt_flags = phdr.p_flags;
	}

	elf_end(elf);

	return pt_flags;
}
#endif


uint16_t
string2bin(char *buf)
{
	uint16_t flags = 0;

	if( buf[0] == 'P' )
		flags |= PF_PAGEEXEC;
	else if( buf[0] == 'p' )
		flags |= PF_NOPAGEEXEC;

	if( buf[1] == 'S' )
		flags |= PF_SEGMEXEC;
	else if( buf[1] == 's' )
		flags |= PF_NOSEGMEXEC;

	if( buf[2] == 'M' )
		flags |= PF_MPROTECT;
	else if( buf[2] == 'm' )
		flags |= PF_NOMPROTECT;

	if( buf[3] == 'E' )
		flags |= PF_EMUTRAMP;
	else if( buf[3] == 'e' )
		flags |= PF_NOEMUTRAMP;

	if( buf[4] == 'R' )
		flags |= PF_RANDMMAP;
	else if( buf[4] == 'r' )
		flags |= PF_NORANDMMAP;

	return flags;
}


#ifdef XTPAX
uint16_t
get_xt_flags(int fd)
{
	char buf[FLAGS_SIZE];
	uint16_t xt_flags = UINT16_MAX;

	memset(buf, 0, FLAGS_SIZE);

	if(fgetxattr(fd, PAX_NAMESPACE, buf, FLAGS_SIZE) != -1)
		xt_flags = string2bin(buf);

	return xt_flags;
}
#endif


void
bin2string(uint16_t flags, char *buf)
{
	buf[0] = flags & PF_PAGEEXEC ? 'P' :
		flags & PF_NOPAGEEXEC ? 'p' : '-' ;

	buf[1] = flags & PF_SEGMEXEC   ? 'S' :
		flags & PF_NOSEGMEXEC ? 's' : '-';

	buf[2] = flags & PF_MPROTECT   ? 'M' :
		flags & PF_NOMPROTECT ? 'm' : '-';

	buf[3] = flags & PF_EMUTRAMP   ? 'E' :
		flags & PF_NOEMUTRAMP ? 'e' : '-';

	buf[4] = flags & PF_RANDMMAP   ? 'R' :
		flags & PF_NORANDMMAP ? 'r' : '-';
}


static PyObject *
pax_getflags(PyObject *self, PyObject *args)
{
	const char *f_name;
	int fd;
	uint16_t flags;
	char buf[FLAGS_SIZE];

	memset(buf, 0, FLAGS_SIZE);

	if (!PyArg_ParseTuple(args, "s", &f_name))
	{
		PyErr_SetString(PaxError, "pax_getflags: PyArg_ParseTuple failed");
		return NULL;
	}

	if((fd = open(f_name, O_RDONLY)) < 0)
	{
		PyErr_SetString(PaxError, "pax_getflags: open() failed");
		return NULL;
	}

	/* Since the xattr pax flags are obtained second, they
	 * will override the PT_PAX flags values.  The pax kernel
	 * expects them to be the same if both PAX_XATTR_PAX_FLAGS
	 * and PAX_PT_PAX_FLAGS else it returns -EINVAL.
	 * (See pax_parse_pax_flags() in fs/binfmt_elf.c.)
	 * Unless migrating, we will document to use one or the
	 * other but not both.
	 */

#ifdef PTPAX
	flags = get_pt_flags(fd);
	if( flags != UINT16_MAX )
	{
		memset(buf, 0, FLAGS_SIZE);
		bin2string(flags, buf);
	}
#endif

#ifdef XTPAX
	flags = get_xt_flags(fd);
	if( flags != UINT16_MAX )
	{
		memset(buf, 0, FLAGS_SIZE);
		bin2string(flags, buf);
	}
#endif

	close(fd);

	return Py_BuildValue("si", buf, flags);
}


#ifdef PTPAX
void
set_pt_flags(int fd, uint16_t pt_flags)
{
	Elf *elf;
	GElf_Phdr phdr;
	size_t i, phnum;

	if(elf_version(EV_CURRENT) == EV_NONE)
	{
		PyErr_SetString(PaxError, "set_pt_flags: library out of date");
		return;
	}

	if((elf = elf_begin(fd, ELF_C_RDWR_MMAP, NULL)) == NULL)
	{
		PyErr_SetString(PaxError, "set_pt_flags: elf_begin() failed");
		return;
	}

	if(elf_kind(elf) != ELF_K_ELF)
	{
		elf_end(elf);
		PyErr_SetString(PaxError, "set_pt_flags: elf_kind() failed: this is not an elf file.");
		return;
	}

	elf_getphdrnum(elf, &phnum);

	for(i=0; i<phnum; i++)
	{
		if(gelf_getphdr(elf, i, &phdr) != &phdr)
		{
			elf_end(elf);
			PyErr_SetString(PaxError, "set_pt_flags: gelf_getphdr() failed");
			return;
		}

		if(phdr.p_type == PT_PAX_FLAGS)
		{
			phdr.p_flags = pt_flags;

			if(!gelf_update_phdr(elf, i, &phdr))
			{
				elf_end(elf);
				PyErr_SetString(PaxError, "set_pt_flags: gelf_update_phdr() failed");
				return;
			}
		}
	}

	elf_end(elf);
}
#endif


#ifdef XTPAX
void
set_xt_flags(int fd, uint16_t xt_flags)
{
	char buf[FLAGS_SIZE];

	memset(buf, 0, FLAGS_SIZE);
	bin2string(xt_flags, buf);
	fsetxattr(fd, PAX_NAMESPACE, buf, strlen(buf), 0);
}
#endif


static PyObject *
pax_setbinflags(PyObject *self, PyObject *args)
{
	const char *f_name;
	int fd, iflags;
	uint16_t flags;

	if (!PyArg_ParseTuple(args, "si", &f_name, &iflags))
	{
		PyErr_SetString(PaxError, "pax_setbinflags: PyArg_ParseTuple failed");
		return NULL;
	}

	if((fd = open(f_name, O_RDWR)) < 0)
	{
		PyErr_SetString(PaxError, "pax_setbinflags: open() failed");
		return NULL;
	}

	flags = (uint16_t) iflags;

#ifdef PTPAX
	set_pt_flags(fd, flags);
#endif

#ifdef XTPAX
	set_xt_flags(fd, flags);
#endif

	close(fd);

	return Py_BuildValue("");
}

static PyObject *
pax_setstrflags(PyObject *self, PyObject *args)
{
	char *f_name, *sflags;
	int fd;
	uint16_t flags;

	if (!PyArg_ParseTuple(args, "ss", &f_name, &sflags))
	{
		PyErr_SetString(PaxError, "pax_setbinflags: PyArg_ParseTuple failed");
		return NULL;
	}

	if((fd = open(f_name, O_RDWR)) < 0)
	{
		PyErr_SetString(PaxError, "pax_setbinflags: open() failed");
		return NULL;
	}

	flags = string2bin(sflags);

#ifdef PTPAX
	set_pt_flags(fd, flags);
#endif

#ifdef XTPAX
	set_xt_flags(fd, flags);
#endif

	close(fd);

	return Py_BuildValue("");
}
