/* mhlogin.c -- login to external (OAuth) services
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <h/mh.h>
#include <h/utils.h>
#include <h/oauth.h>

#define MHLOGIN_SWITCHES \
    X("user username", 0, USERSW) \
    X("saslmech", 0, SASLMECHSW) \
    X("authservice", 0, AUTHSERVICESW) \
    X("browser", 0, BROWSERSW) \
    X("snoop", 0, SNOOPSW) \
    X("help", 0, HELPSW) \
    X("version", 0, VERSIONSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHLOGIN);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHLOGIN, switches);
#undef X

#ifdef OAUTH_SUPPORT
/* XXX copied from install-mh.c */
static char *
geta (void)
{
    static char line[BUFSIZ];

    if (fgets(line, sizeof(line), stdin) == NULL)
	done (1);
    trim_suffix_c(line, '\n');

    return line;
}

static int
do_login(const char *svc, const char *user, const char *browser, int snoop)
{
    char *fn, *code;
    mh_oauth_ctx *ctx;
    mh_oauth_cred *cred;
    FILE *cred_file;
    int failed_to_lock = 0;
    const char *url;

    if (svc == NULL) {
        adios(NULL, "missing -authservice switch");
    }

    if (user == NULL) {
        adios(NULL, "missing -user switch");
    }

    if (!mh_oauth_new(&ctx, svc)) {
        adios(NULL, mh_oauth_get_err_string(ctx));
    }

    if (snoop) {
        mh_oauth_log_to(stderr, ctx);
    }

    fn = mh_xstrdup(mh_oauth_cred_fn(svc));

    if ((url = mh_oauth_get_authorize_url(ctx)) == NULL) {
      adios(NULL, mh_oauth_get_err_string(ctx));
    }

    if (browser) {
        char *command = concat(browser, " '", url, "'", NULL);
        int status = OK;

        printf("Follow the prompts in your browser to authorize nmh"
               " to access %s.\n",
               mh_oauth_svc_display_name(ctx));
        sleep(1);

        status = system(command);
        free(command);

        if (status != OK) {
            adios ((char *) browser, "SYSTEM");
        }
    } else {
        printf("Load the following URL in your browser and authorize nmh"
               " to access %s:\n\n%s\n\n",
               mh_oauth_svc_display_name(ctx), url);
    }
    printf("Enter the authorization code: ");
    fflush(stdout);
    code = geta();

    while (!*code ||
           ((cred = mh_oauth_authorize(code, ctx)) == NULL
            && mh_oauth_get_err_code(ctx) == MH_OAUTH_BAD_GRANT)) {
      printf(!*code  ?  "Empty code; try again? "  :  "Code rejected; try again? ");
      fflush(stdout);
      code = geta();
    }
    if (cred == NULL) {
      inform("error exchanging code for OAuth2 token");
      adios(NULL, mh_oauth_get_err_string(ctx));
    }

    cred_file = lkfopendata(fn, "r+", &failed_to_lock);
    if (cred_file == NULL && errno == ENOENT) {
        cred_file = lkfopendata(fn, "w+", &failed_to_lock);
    }
    if (cred_file == NULL || failed_to_lock) {
      adios(fn, "oops");
    }
    if (!mh_oauth_cred_save(cred_file, cred, user)) {
      adios(NULL, mh_oauth_get_err_string(ctx));
    }
    if (lkfclosedata(cred_file, fn) != 0) {
      adios (fn, "oops");
    }

    free(fn);
    mh_oauth_cred_free(cred);
    mh_oauth_free(ctx);

    return 0;
}
#endif

int
main(int argc, char **argv)
{
    char *cp, **argp, **arguments;
    const char *user = NULL, *saslmech = NULL, *svc = NULL, *browser = NULL;
    int snoop = 0;

    if (nmh_init(argv[0], 1)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
            char help[BUFSIZ];
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW:
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW:
		adios (NULL, "-%s unknown", cp);

	    case HELPSW:
		snprintf(help, sizeof(help), "%s [switches]",
                         invo_name);
		print_help (help, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case USERSW:
		if (!(user = *argp++) || *user == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case SASLMECHSW:
		if (!(saslmech = *argp++) || *saslmech == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case AUTHSERVICESW:
                if (!(svc = *argp++) || *svc == '-')
                    adios (NULL, "missing argument to %s", argp[-2]);
                continue;

	    case BROWSERSW:
                if (!(browser = *argp++) || *browser == '-')
                    adios (NULL, "missing argument to %s", argp[-2]);
                continue;

            case SNOOPSW:
                snoop++;
                continue;
	    }
	}
        adios(NULL, "extraneous arguments");
    }

    if (saslmech  &&  strcasecmp(saslmech, "xoauth2")) {
        /* xoauth is assumed */
        adios(NULL, "only -saslmech xoauth2 is supported");
    }
    free(arguments);

#ifdef OAUTH_SUPPORT
    return do_login(svc, user, browser, snoop);
#else
    NMH_UNUSED(svc);
    NMH_UNUSED(browser);
    NMH_UNUSED(snoop);
    adios(NULL, "not built with OAuth support");
    return 1;
#endif
}
