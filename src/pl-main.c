/*  $Id$

    Copyright (c) 1990 Jan Wielemaker. All rights reserved.
    See ../LICENCE to find out about your rights.
    jan@swi.psy.uva.nl

    Purpose: Prologs main module
*/

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Get the ball rolling.  The main task of  this  module  is  command  line
option  parsing,  initialisation  and  handling  of errors and warnings.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*#define O_DEBUG 1*/

#ifdef __WINDOWS__
#include <windows.h>
#undef TRANSPARENT
#endif

#include "pl-incl.h"
#include "pl-itf.h"
#include "pl-save.h"
#if unix || EMX
#include <sys/param.h>
#endif

forwards void	usage(void);
forwards char * findHome(char *);
forwards char *	findState();

#define	optionString(s) { if (argc > 1) \
			  { s = argv[1]; argc--; argv++; \
			  } else \
			    usage(); \
			}

static char *
findHome(def)
char *def;
{ char *home;

  if ( !(home = getenv("SWI_HOME_DIR")) )
    home = getenv("SWIPL");
  if ( home && (home = PrologPath(home)) && ExistsDirectory(home) )
    return store_string(home);

  if ( (home = Symbols()) )
  { char magic[MAXPATHLEN];
    home = DirName(DirName(AbsoluteFile(home)));

    sprintf(magic, "%s/swipl", home);
    if ( ExistsFile(magic) )
    { FILE *fd = Fopen(magic, "r");

      if ( fd && fgets(magic, sizeof(magic), fd) )
      { int l = strlen(magic);
	if ( l > 0 && l < sizeof(magic) && magic[l-1] == '\n' )
	  magic[l-1] = EOS;

	home = AbsoluteFile(magic);
	if ( ExistsDirectory(home) )
	{ fclose(fd);
	  return store_string(home);
	}
      }
      if ( fd )
	fclose(fd);
    }
  }

  if ( ExistsDirectory(def) )
    return def;

#if tos || __DOS__ || __WINDOWS__
#if tos
#define HasDrive(c) (Drvmap() & (1 << (c - 'a')))
#else
#define HasDrive(c) 1
#endif
  { char *drv;
    static char drvs[] = "cdefghijklmnopab";
    char home[MAXPATHLEN];

    for(drv = drvs; *drv; drv++)
    { sprintf(home, "/%c:/pl", *drv);
      if ( HasDrive(*drv) && ExistsDirectory(home) )
        return store_string(home);
    }
  }
#endif

  fatalError("Can't find Prolog Home Directory");
  return (char *) NULL;
}

/*

  -- atoenne -- convert state to an absolute path. This allows relative
  SWI_HOME_DIR and cleans up non-canonical paths.
*/

#ifndef IS_DIR_SEPARATOR
#define IS_DIR_SEPARATOR(c) ((c) == '/')
#endif

static char *
proposeStartupFile()
{ char state[MAXPATHLEN];
  char *symbols = Symbols();

  if ( symbols )
  { char *s, *dot = NULL;

    strcpy(state, symbols);
    for(s=state; *s; s++)
    { if ( *s == '.' )
	dot = s;
      if ( IS_DIR_SEPARATOR(*s) )
	dot = NULL;
    }
    if ( dot )
      *dot = EOS;

    strcat(state, ".qlf");

    return store_string(state);
  }

  sprintf(state, "%s/startup/startup.%s",
	  systemDefaults.home, systemDefaults.machine);

  return store_string(AbsoluteFile(state));
}


static char *
findState()
{ char state[MAXPATHLEN];
  char *full;

  full = proposeStartupFile();
  if ( ExistsFile(state) )
    return full;

  sprintf(state, "%s/startup/startup.%s",
	  systemDefaults.home, systemDefaults.machine);
  full = AbsoluteFile(state);
  if ( ExistsFile(full) )
    return store_string(full);

  sprintf(state, "%s/startup/startup", systemDefaults.home);
  full = AbsoluteFile(state);
  if ( ExistsFile(full) )
    return store_string(full);

  return proposeStartupFile();		/* to give error */
}


#if O_LINK_PCE
foreign_t
pl_pce_init()
{ prolog_pce_init(mainArgc, mainArgv);

  succeed;
}
#endif

int
startProlog(argc, argv, env)
int argc;
char **argv;
char **env;
{ char *s;
  int n;
  char *state;
  bool compile;
  bool explicit_compile_out = FALSE;

  mainArgc			= argc;
  mainArgv			= argv;
  mainEnv			= env;

#if O_MALLOC_DEBUG
  malloc_debug(O_MALLOC_DEBUG);
#endif

 /* status.debugLevel = 9; */

#ifndef MACHINE_ID
#define MACHINE_ID MACHINE
#endif

  if ( status.dumped == FALSE )
  { systemDefaults.machine	    = MACHINE_ID;
    systemDefaults.home      = findHome(store_string(PrologPath(SYSTEMHOME)));
    systemDefaults.state	    = findState();
    systemDefaults.startup	    = store_string(PrologPath(DEFSTARTUP));
    systemDefaults.version	    = store_string(PLVERSION);
    systemDefaults.local	    = DEFLOCAL;
    systemDefaults.global	    = DEFGLOBAL;
    systemDefaults.trail	    = DEFTRAIL;
    systemDefaults.argument	    = DEFARGUMENT;
    systemDefaults.lock		    = DEFLOCK;
    systemDefaults.goal		    = "'$welcome'";
    systemDefaults.toplevel	    = "prolog";
    systemDefaults.notty	    = FALSE;
    systemDefaults.operating_system = OPERATING_SYSTEM;

  } else
  { DEBUG(1, printf("Restarting from dumped state\n"));
  }

  compile			= FALSE;
  state				= systemDefaults.state;
  status.io_initialised		= FALSE;
  status.initialised		= FALSE;
  status.notty			= systemDefaults.notty;
  status.boot			= FALSE;
  status.extendMode		= TRUE;

  argc--; argv++;

#if O_RLC				/* MS-Windows readline console */
  { char title[60];

    sprintf(title, "SWI-Prolog (version %s)", PLVERSION);
    rlc_title(title);
  }
#endif
					/* EMACS inferior processes */
					/* PceEmacs inferior processes */
  if ( ((s = getenv("EMACS")) != NULL && streq(s, "t")) ||
       ((s = getenv("INFERIOR")) != NULL && streq(s, "yes")) )
    status.notty = TRUE;

  for(n=0; n<argc; n++)			/* need to check this first */
  { DEBUG(2, printf("argv[%d] = %s\n", n, argv[n]));
    if (streq(argv[n], "-b") )
      status.boot = TRUE;
  }

  DEBUG(1, {if (status.boot) printf("Boot session\n");});

  if ( argc >= 2 && streq(argv[0], "-r") )
  { loaderstatus.restored_state = lookupAtom(AbsoluteFile(argv[1]));
    argc -= 2, argv += 2;		/* recover; we've done this! */
  }

  if ( argc >= 2 && streq(argv[0], "-x") )
  { state = argv[1];
    argc -= 2, argv += 2;
    DEBUG(1, printf("Startup file = %s\n", state));
  }

  if ( argc >= 1 && streq(argv[0], "-help") )
    usage();

#define K * 1024L

  if ( state != NULL && status.boot == FALSE )
  { DEBUG(1, printf("Scanning %s for options\n", state));
    if ( loadWicFile(state, TRUE, TRUE) == FALSE )
      Halt(1);
    DEBUG(2, printf("options.localSize    = %ld\n", options.localSize));
    DEBUG(2, printf("options.globalSize   = %ld\n", options.globalSize));
    DEBUG(2, printf("options.trailSize    = %ld\n", options.trailSize));
    DEBUG(2, printf("options.argumentSize = %ld\n", options.argumentSize));
    DEBUG(2, printf("options.lockSize	  = %ld\n", options.lockSize));
    DEBUG(2, printf("options.goal         = %s\n",  options.goal));
    DEBUG(2, printf("options.topLevel     = %s\n",  options.topLevel));
    DEBUG(2, printf("options.initFile     = %s\n",  options.initFile));
  } else
  { options.compileOut	  = "a.out";
    options.localSize	  = systemDefaults.local    K;
    options.globalSize	  = systemDefaults.global   K;
    options.trailSize	  = systemDefaults.trail    K;
    options.argumentSize  = systemDefaults.argument K;
    options.lockSize	  = systemDefaults.lock	    K;
    options.goal	  = systemDefaults.goal;
    options.topLevel	  = systemDefaults.toplevel;
    options.initFile      = systemDefaults.startup;
  }

  for( ; argc > 0 && (argv[0][0] == '-' || argv[0][0] == '+'); argc--, argv++ )
  { if ( streq(&argv[0][1], "tty") )
    { status.notty = (argv[0][0] == '-');
      continue;
    }

    s = &argv[0][1];
    while(*s)
    { switch(*s)
      { case 'd':	if (argc > 1)
			{ status.debugLevel = atoi(argv[1]);
			  argc--, argv++;
			} else
			  usage();
			break;
	case 'O':	status.optimise = TRUE;
			break;
  	case 'o':	optionString(options.compileOut);
			explicit_compile_out = TRUE;
			break;
	case 'f':	optionString(options.initFile);
			break;
	case 'g':	optionString(options.goal);
			break;
	case 't':	optionString(options.topLevel);
			break;
	case 'c':	compile = TRUE;
			break;
	case 'b':	status.boot = TRUE;
			break;
	case 'B':
#if !O_DYNAMIC_STACKS
			options.localSize    = 32 K;
			options.globalSize   = 8 K;
			options.trailSize    = 8 K;
			options.argumentSize = 1 K;
			options.lockSize     = 1 K;
#endif
			goto next;
	case 'L':	options.localSize    = atoi(++s) K; goto next;
	case 'G':	options.globalSize   = atoi(++s) K; goto next;
	case 'T':	options.trailSize    = atoi(++s) K; goto next;
	case 'A':	options.argumentSize = atoi(++s) K; goto next;
	case 'P':	options.lockSize     = atoi(++s) K; goto next;
      }
      s++;
    }
    next:;
  }
#undef K
  
  DEBUG(1, printf("Command line options parsed\n"));

  setupProlog();
  systemMode(TRUE);

  if ( status.boot )
  { if ( !explicit_compile_out )
      options.compileOut = proposeStartupFile();

    if ( compileFileList(options.compileOut, argc, argv) == TRUE )
    {
#ifdef __WINDOWS__
      char msg[200];
      sprintf(msg, "Boot compilation has created %s", options.compileOut);
      MessageBox(NULL, msg, "SWI-Prolog", MB_OK|MB_TASKMODAL);
#endif
      Halt(0);
    }

    Halt(1);
  }

  if ( state != NULL )
  { status.boot = TRUE;
    if ( loadWicFile(state, TRUE, FALSE) == FALSE )
      Halt(1);
    status.boot = FALSE;
  }

  if ( PL_foreign_reinit_function != NULL )
    (*PL_foreign_reinit_function)(mainArgc, mainArgv);

  systemMode(FALSE);
  status.dumped = TRUE;
  status.initialised = TRUE;

#if O_LINK_PCE
  PL_register_foreign("$pce_init", 0, pl_pce_init, PL_FA_TRANSPARENT, 0);
#endif

  DEBUG(1, printf("Starting Prolog Engine\n"));

  if ( prolog(PL_new_atom(compile ? "$compile" : "$init")) == TRUE )
    Halt(0);
  else
    Halt(1);

  return 0;
}

static void
usage()
{ static char *lines[] = {
    "%s: Usage:\n",
    "    1) %s -help\n",
    "    2) %s [options]\n",
    "    3) %s [options] [-o output] -c file ...\n",
    "    4) %s [options] [-o output] -b bootfile -c file ...\n",
    "Options:\n",
    "    -x state        Start from state (must be first)\n",
    "    -[LGTAP]kbytes  Specify {Local,Global,Trail,Argument,Lock} stack sizes\n",
    "    -B              Small stack sizes to prepare for boot\n",
    "    -t toplevel     Toplevel goal\n",
    "    -g goal         Initialisation goal\n",
    "    -f file         Initialisation file\n",
    "    [+/-]tty        Allow tty control\n",
    "    -O              Optimised compilation\n",
    NULL
  };
  char **lp = lines;

  for(lp = lines; *lp; lp++)
    fprintf(stderr, *lp, BaseName(mainArgv[0]));

  Halt(1);
}

#include <stdarg.h>

bool
sysError(char *fm, ...)
{ va_list args;

  va_start(args, fm);
  vsysError(fm, args);
  va_end(args);

  PL_fail;
}


bool
fatalError(char *fm, ...)
{ va_list args;

  va_start(args, fm);
  vfatalError(fm, args);
  va_end(args);

  PL_fail;
}


bool
warning(char *fm, ...)
{ va_list args;

  va_start(args, fm);
  vwarning(fm, args);
  va_end(args);

  PL_fail;
}


bool
vsysError(fm, args)
char *fm;
va_list args;
{ fprintf(stderr, "[PROLOG INTERNAL ERROR:\n\t");
  vfprintf(stderr, fm, args);
  if ( gc_status.active )
  { fprintf(stderr,
	    "\n[While in %ld-th garbage collection; skipping stacktrace]\n",
	    gc_status.collections);
  } else
  { fprintf(stderr, "\n[Switched to system mode: style_check(+dollar)]\n");
    debugstatus.styleCheck |= DOLLAR_STYLE;
    fprintf(stderr, "PROLOG STACK:\n");
    backTrace(NULL, 10);
    fprintf(stderr, "]\n");
  }

  pl_abort();
  Halt(3);
  PL_fail;
}

bool
vfatalError(fm, args)
char *fm;
va_list args;
{
#ifdef __WINDOWS__
  char msg[500];
  sprintf(msg, "[FATAL ERROR:\n\t");
  vsprintf(&msg[strlen(msg)], fm, args);
  sprintf(&msg[strlen(msg)], "]");
  MessageBox(NULL, msg, "Error", MB_OK);
#else
  fprintf(stderr, "[FATAL ERROR:\n\t");
  vfprintf(stderr, fm, args);
  fprintf(stderr, "]\n");
#endif

  Halt(2);
  PL_fail;
}

bool
vwarning(fm, args)
char *fm;
va_list args;
{ toldString();

  if ( ReadingSource && !status.boot && status.initialised )
  { word goal;
    char message[LINESIZ];
    word arg;
    mark m;

    vsprintf(message, fm, args);

    Mark(m);
    goal = globalFunctor(FUNCTOR_exception3);
    unifyAtomic(argTermP(goal, 0), ATOM_warning);
    unifyFunctor(argTermP(goal, 1), FUNCTOR_warning3);
    arg = argTerm(goal, 1);
    unifyAtomic(argTermP(arg, 0), source_file_name);
    unifyAtomic(argTermP(arg, 1), consNum(source_line_no));
    unifyAtomic(argTermP(arg, 2), globalString(message));

    if ( callGoal(MODULE_user, goal, FALSE) == FALSE )
    { extern int Output;
      int old = Output;
    
      Output = 2;
      Putf("[WARNING: (%s:%d)\n\t%s]\n",
	    stringAtom(source_file_name), source_line_no, message);
      Output = old;
    }
    Undo(m);

    PL_fail;				/* handled */
  }


  if ( status.io_initialised )
  { extern int Output;
    int old = Output;
    
    Output = 2;
    Putf("[WARNING: ");
    vPutf(fm, args);
    Putf("]\n");
    Output = old;
  } else
  { fprintf(stderr, "[WARNING: ");
    vfprintf(stderr, fm, args);
    fprintf(stderr, "]\n");
  }

  pl_trace();

  PL_fail;
}
