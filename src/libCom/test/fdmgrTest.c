
#include <stdio.h>
#include <math.h>

#include "fdmgr.h"
#include "osiTime.h"
#include "cadef.h"
#include "epicsAssert.h"

static const unsigned uSecPerSec = 1000000;

typedef struct cbStructCreateDestroyFD {
    fdctx *pfdm;
    int trig;
} cbStructCreateDestroyFD;

void fdHandler (void *pArg)
{
    cbStructCreateDestroyFD *pCBFD = (cbStructCreateDestroyFD *) pArg;

    printf ("triggered\n");
    pCBFD->trig = 1;
}

void fdCreateDestroyHandler (void *pArg, int fd, int open)
{
    cbStructCreateDestroyFD *pCBFD = (cbStructCreateDestroyFD *) pArg;
    int status;

    if (open) {
        printf ("new fd = %d\n", fd);
        status = fdmgr_add_callback (pCBFD->pfdm, fd, fdi_read, fdHandler, pArg);
        assert (status==0);
    }
    else {
        printf ("terminated fd = %d\n", fd);
        status = fdmgr_clear_callback (pCBFD->pfdm, fd, fdi_read);
        assert (status==0);
    }
}

typedef struct cbStuctTimer {
    osiTime time;
    int done;
} cbStruct;

void alarmCB (void *parg)
{
    cbStruct *pCBS = (cbStruct *) parg;
    osiTimeGetCurrent (&pCBS->time);
    pCBS->done = 1;
}

void testTimer (fdctx *pfdm, long double delay)
{
    int status;
    fdmgrAlarmId aid;
    struct timeval tmo;
    osiTime begin;
    cbStruct cbs;
    long double measuredDelay;
    long double measuredError;

    osiTimeGetCurrent (&begin);
    cbs.done = 0;
    tmo.tv_sec = (unsigned long) delay;
    tmo.tv_usec = (unsigned long) ((delay - tmo.tv_sec) * uSecPerSec);
    aid = fdmgr_add_timeout (pfdm, &tmo, alarmCB, &cbs);
    assert (aid!=fdmgrNoAlarm);

    while (!cbs.done) {
        tmo.tv_sec = (unsigned long) delay;
        tmo.tv_usec = (unsigned long) ((delay - tmo.tv_sec) * uSecPerSec);
        status = fdmgr_pend_event (pfdm, &tmo);
        assert (status==0);
    }

    measuredDelay = osiTimeDiffInSeconds (&cbs.time, &begin);
    measuredError = fabs (measuredDelay-delay);
    printf ("measured delay for %lf sec was off by %lf sec (%lf %%)\n", 
        delay, measuredError, 100.0*measuredError/delay);
}

main (int argc, char **argv)
{
    int status;
    fdctx *pfdm;
    cbStructCreateDestroyFD cbsfd;
    struct timeval tmo;
    chid chan;

    pfdm = fdmgr_init ();
    assert (pfdm);

    SEVCHK (ca_task_initialize(), NULL);
    cbsfd.pfdm = pfdm;
    SEVCHK (ca_add_fd_registration (fdCreateDestroyHandler, &cbsfd), NULL);

    /*
     * timer test
     */
    testTimer (pfdm, 0.001);
    testTimer (pfdm, 0.01);
    testTimer (pfdm, 0.1);
    testTimer (pfdm, 1.0);

    if (argc==2) {
         SEVCHK(ca_search (argv[1], &chan), NULL);
    }

    while (1) {
        tmo.tv_sec = 0;
        tmo.tv_usec = 100000;
        cbsfd.trig = 0;
        status = fdmgr_pend_event (pfdm, &tmo);
        assert (status==0);
        ca_poll ();
    }

    status = fdmgr_delete (pfdm);
    assert (status==0);
}

