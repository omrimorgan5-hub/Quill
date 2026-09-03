#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static char *quill_concat(const char *a, const char *b)
{
    size_t lenA = a ? strlen(a) : 0;
    size_t lenB = b ? strlen(b) : 0;
    char *result = (char *)malloc(lenA + lenB + 1);
    if (!result)
    {
        return NULL;
    }
    if (lenA > 0)
        memcpy(result, a, lenA);
    if (lenB > 0)
        memcpy(result + lenA, b, lenB);
    result[lenA + lenB] = '\0';
    return result;
}

static char *quill_dup(const char *s)
{
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out)
    {
        return NULL;
    }
    memcpy(out, s, len + 1);
    return out;
}

static char *quill_itoa(long long v)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lld", v);
    return quill_dup(buffer);
}

static char *quill_ftoa(double v)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%f", v);
    return quill_dup(buffer);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

typedef struct
{
    char host[64];
    char user[128];
    char repo[128];
    char ref[128];
} DepSpec;

static void dep_full_name(const DepSpec *d, char *out, size_t n)
{
    snprintf(out, n, "%s/%s", d->user, d->repo);
}

static void dep_string(const DepSpec *d, char *out, size_t n)
{
    const char *ref = d->ref[0] ? d->ref : "latest";
    snprintf(out, n, "%s:%s/%s#%s", d->host, d->user, d->repo, ref);
}

static int parse_spec(const char *s, DepSpec *out)
{
    char buf[512];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *p = buf;
    while (*p == ' ' || *p == '\t')
        p++;
    char *end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t'))
        *--end = '\0';

    char host[64] = "github";
    char *rest = p;
    char *colon = strchr(p, ':');
    if (colon)
    {
        *colon = '\0';
        strncpy(host, p, sizeof(host) - 1);
        rest = colon + 1;
    }

    char ref[128] = "";
    char *hash = strchr(rest, '#');
    if (hash)
    {
        *hash = '\0';
        strncpy(ref, hash + 1, sizeof(ref) - 1);
    }

    char *slash = strchr(rest, '/');
    if (!slash || slash == rest || !*(slash + 1) || strchr(slash + 1, '/'))
        return -1;
    *slash = '\0';

    memset(out, 0, sizeof(*out));
    strncpy(out->host, host, sizeof(out->host) - 1);
    strncpy(out->user, rest, sizeof(out->user) - 1);
    strncpy(out->repo, slash + 1, sizeof(out->repo) - 1);
    strncpy(out->ref, ref, sizeof(out->ref) - 1);
    return 0;
}

static int path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static int run_cmd(const char *cmd)
{
    return system(cmd) == 0 ? 0 : -1;
}

static int install_dep(const DepSpec *dep, const char *modulesDir)
{
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/%s/%s", modulesDir, dep->user, dep->repo);

    if (path_exists(dest))
    {
        fprintf(stderr, "%s/%s already exists — run 'qpm update' instead\n",
                dep->user, dep->repo);
        return -1;
    }

    char parent[512];
    snprintf(parent, sizeof(parent), "%s/%s", modulesDir, dep->user);
    char mkdir_cmd[640];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", parent);
    if (run_cmd(mkdir_cmd) != 0)
        return -1;

    char url[512];
    snprintf(url, sizeof(url), "https://%s.com/%s/%s.git",
             dep->host, dep->user, dep->repo);

    char clone_cmd[2048];
    if (dep->ref[0])
    {
        snprintf(clone_cmd, sizeof(clone_cmd),
                 "git clone --depth 1 --branch '%s' '%s' '%s'",
                 dep->ref, url, dest);
    }
    else
    {
        snprintf(clone_cmd, sizeof(clone_cmd),
                 "git clone --depth 1 '%s' '%s'", url, dest);
    }

    if (run_cmd(clone_cmd) != 0)
    {
        char rm_cmd[640];
        snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf '%s'", dest);
        run_cmd(rm_cmd);
        fprintf(stderr, "git clone failed\n");
        return -1;
    }
    return 0;
}

static int update_dep(const DepSpec *dep, const char *modulesDir)
{
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/%s/%s", modulesDir, dep->user, dep->repo);

    if (!path_exists(dest))
        return install_dep(dep, modulesDir);

    char pull_cmd[640];
    snprintf(pull_cmd, sizeof(pull_cmd), "git -C '%s' pull --ff-only", dest);
    if (run_cmd(pull_cmd) != 0)
    {
        fprintf(stderr, "git pull failed\n");
        return -1;
    }

    if (dep->ref[0])
    {
        char co_cmd[720];
        snprintf(co_cmd, sizeof(co_cmd),
                 "git -C '%s' checkout '%s'", dest, dep->ref);
        if (run_cmd(co_cmd) != 0)
        {
            fprintf(stderr, "checkout failed\n");
            return -1;
        }
    }
    return 0;
}

/* Quill-facing API */
int deps_parse_ok(const char *spec)
{
    DepSpec d;
    return parse_spec(spec, &d) == 0 ? 1 : 0;
}

int deps_install(const char *spec, const char *modulesDir)
{
    DepSpec d;
    if (parse_spec(spec, &d) != 0)
        return -1;
    return install_dep(&d, modulesDir);
}

int deps_update(const char *spec, const char *modulesDir)
{
    DepSpec d;
    if (parse_spec(spec, &d) != 0)
        return -1;
    return update_dep(&d, modulesDir);
}

/* Fills out buffers with user/repo and full string form. */
int deps_info(const char *spec, char *full_out, int full_n, char *str_out, int str_n)
{
    DepSpec d;
    if (parse_spec(spec, &d) != 0)
        return -1;
    dep_full_name(&d, full_out, (size_t)full_n);
    dep_string(&d, str_out, (size_t)str_n);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MANIFEST_MAX_DEPS 64
#define MANIFEST_STR 256

typedef struct
{
    char key[MANIFEST_STR];
    char val[MANIFEST_STR];
} DepEntry;

typedef struct
{
    char name[MANIFEST_STR];
    char version[MANIFEST_STR];
    char main[MANIFEST_STR];
    DepEntry deps[MANIFEST_MAX_DEPS];
    int dep_count;
} Manifest;

static Manifest g_manifest;
static int g_loaded = 0;

static void json_get_string(const char *json, const char *key, char *out, size_t out_n)
{
    out[0] = '\0';
    char pattern[MANIFEST_STR + 8];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p)
        return;
    p = strchr(p + strlen(pattern), '"');
    if (!p)
        return;
    p++;
    const char *q = strchr(p, '"');
    if (!q)
        return;
    size_t len = (size_t)(q - p);
    if (len >= out_n)
        len = out_n - 1;
    memcpy(out, p, len);
    out[len] = '\0';
}

static void json_get_deps(const char *json, Manifest *m)
{
    m->dep_count = 0;
    const char *deps = strstr(json, "\"deps\"");
    if (!deps)
        return;
    const char *brace = strchr(deps, '{');
    if (!brace)
        return;
    const char *end = strchr(brace, '}');
    if (!end)
        return;

    const char *p = brace + 1;
    while (p < end && m->dep_count < MANIFEST_MAX_DEPS)
    {
        const char *k1 = strchr(p, '"');
        if (!k1 || k1 >= end)
            break;
        k1++;
        const char *k2 = strchr(k1, '"');
        if (!k2 || k2 >= end)
            break;
        const char *v1 = strchr(k2 + 1, '"');
        if (!v1 || v1 >= end)
            break;
        v1++;
        const char *v2 = strchr(v1, '"');
        if (!v2 || v2 >= end)
            break;

        size_t klen = (size_t)(k2 - k1);
        size_t vlen = (size_t)(v2 - v1);
        if (klen >= MANIFEST_STR)
            klen = MANIFEST_STR - 1;
        if (vlen >= MANIFEST_STR)
            vlen = MANIFEST_STR - 1;

        memcpy(m->deps[m->dep_count].key, k1, klen);
        m->deps[m->dep_count].key[klen] = '\0';
        memcpy(m->deps[m->dep_count].val, v1, vlen);
        m->deps[m->dep_count].val[vlen] = '\0';
        m->dep_count++;
        p = v2 + 1;
    }
}

static int read_file(const char *path, char *buf, size_t buf_n)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, buf_n - 1, f);
    buf[n] = '\0';
    fclose(f);
    return 0;
}

static int write_file(const char *path, const char *data)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    fputs(data, f);
    fputc('\n', f);
    fclose(f);
    return 0;
}

int manifest_read(const char *path)
{
    char data[16384];
    if (read_file(path, data, sizeof(data)) != 0)
    {
        if (errno == ENOENT)
            fprintf(stderr, "no quill.json found — run 'qpm init' first\n");
        else
            fprintf(stderr, "failed to read %s\n", path);
        g_loaded = 0;
        return -1;
    }
    memset(&g_manifest, 0, sizeof(g_manifest));
    json_get_string(data, "name", g_manifest.name, sizeof(g_manifest.name));
    json_get_string(data, "version", g_manifest.version, sizeof(g_manifest.version));
    json_get_string(data, "main", g_manifest.main, sizeof(g_manifest.main));
    json_get_deps(data, &g_manifest);
    g_loaded = 1;
    return 0;
}

int manifest_write(const char *path)
{
    if (!g_loaded)
    {
        if (!g_manifest.name[0])
            strcpy(g_manifest.name, "");
        if (!g_manifest.version[0])
            strcpy(g_manifest.version, "0.1.0");
        if (!g_manifest.main[0])
            strcpy(g_manifest.main, "main.qsc");
        g_loaded = 1;
    }
    char out[16384];
    size_t pos = 0;
    pos += (size_t)snprintf(out + pos, sizeof(out) - pos,
                            "{\n  \"name\": \"%s\",\n  \"version\": \"%s\",\n  \"main\": \"%s\",\n  \"deps\": {",
                            g_manifest.name, g_manifest.version, g_manifest.main);
    for (int i = 0; i < g_manifest.dep_count; i++)
    {
        pos += (size_t)snprintf(out + pos, sizeof(out) - pos,
                                "%s\n    \"%s\": \"%s\"",
                                (i == 0 ? "" : ","),
                                g_manifest.deps[i].key,
                                g_manifest.deps[i].val);
    }
    pos += (size_t)snprintf(out + pos, sizeof(out) - pos, "\n  }\n}");
    if (write_file(path, out) != 0)
    {
        fprintf(stderr, "failed to write %s\n", path);
        return -1;
    }
    return 0;
}

void manifest_clear(void)
{
    memset(&g_manifest, 0, sizeof(g_manifest));
    g_loaded = 0;
}

void manifest_set_name(const char *s)
{
    strncpy(g_manifest.name, s ? s : "", sizeof(g_manifest.name) - 1);
    g_manifest.name[sizeof(g_manifest.name) - 1] = '\0';
    g_loaded = 1;
}
void manifest_set_version(const char *s)
{
    strncpy(g_manifest.version, s ? s : "", sizeof(g_manifest.version) - 1);
    g_manifest.version[sizeof(g_manifest.version) - 1] = '\0';
    g_loaded = 1;
}
void manifest_set_main(const char *s)
{
    strncpy(g_manifest.main, s ? s : "", sizeof(g_manifest.main) - 1);
    g_manifest.main[sizeof(g_manifest.main) - 1] = '\0';
    g_loaded = 1;
}

int manifest_set_dep(const char *key, const char *val)
{
    if (!key || !val)
        return -1;
    g_loaded = 1;
    for (int i = 0; i < g_manifest.dep_count; i++)
    {
        if (strcmp(g_manifest.deps[i].key, key) == 0)
        {
            strncpy(g_manifest.deps[i].val, val, sizeof(g_manifest.deps[i].val) - 1);
            g_manifest.deps[i].val[sizeof(g_manifest.deps[i].val) - 1] = '\0';
            return 0;
        }
    }
    if (g_manifest.dep_count >= MANIFEST_MAX_DEPS)
        return -1;
    strncpy(g_manifest.deps[g_manifest.dep_count].key, key, sizeof(g_manifest.deps[0].key) - 1);
    strncpy(g_manifest.deps[g_manifest.dep_count].val, val, sizeof(g_manifest.deps[0].val) - 1);
    g_manifest.deps[g_manifest.dep_count].key[sizeof(g_manifest.deps[0].key) - 1] = '\0';
    g_manifest.deps[g_manifest.dep_count].val[sizeof(g_manifest.deps[0].val) - 1] = '\0';
    g_manifest.dep_count++;
    return 0;
}

int manifest_dep_count(void)
{
    return g_loaded ? g_manifest.dep_count : 0;
}

/* Returns 1 if index valid; writes key/val into buffers. */
int manifest_dep_at(int i, char *key_out, int key_n, char *val_out, int val_n)
{
    if (!g_loaded || i < 0 || i >= g_manifest.dep_count)
        return 0;
    snprintf(key_out, (size_t)key_n, "%s", g_manifest.deps[i].key);
    snprintf(val_out, (size_t)val_n, "%s", g_manifest.deps[i].val);
    return 1;
}

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

/* argv bridge: Quill's generated main is main(void), so a small C
   forwarder sets these before calling qpm_run(). */
static int g_argc = 0;
static char **g_argv = 0;

void qpm_set_args(int argc, char **argv)
{
    g_argc = argc;
    g_argv = argv;
}

int qpm_argc(void) { return g_argc; }

/* Returns argv[i] or empty string if out of range */
const char *qpm_arg(int i)
{
    if (!g_argv || i < 0 || i >= g_argc)
        return "";
    return g_argv[i] ? g_argv[i] : "";
}

void qpm_print_usage(void)
{
    puts(
        "qpm — Quill Package Manager\n"
        "\n"
        "Usage:\n"
        "  qpm init                Create quill.json in current directory\n"
        "  qpm add <spec>          Add a dependency (e.g., qpm add omri/math)\n"
        "  qpm install             Install all dependencies from quill.json\n"
        "  qpm update              Update all dependencies to latest\n"
        "  qpm list                List installed dependencies\n"
        "\n"
        "Dependency spec format:\n"
        "  user/repo               Short form (uses github, latest)\n"
        "  user/repo#v1.2.0        Pin to tag or commit\n"
        "  github:user/repo#abc    Full form with host and ref");
}

/* cwd base name for init */
void qpm_cwd_base(char *out, int n)
{
    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd)))
    {
        snprintf(out, (size_t)n, "project");
        return;
    }
    char *base = basename(cwd);
    snprintf(out, (size_t)n, "%s", base ? base : "project");
}

bool parse_spec_ok(const char *s)
{
    return (deps_parse_ok(s) == 1);
}

int install(const char *spec, const char *modulesDir)
{
    return deps_install(spec, modulesDir);
}

int update(const char *spec, const char *modulesDir)
{
    return deps_update(spec, modulesDir);
}

int read_manifest(const char *path)
{
    return manifest_read(path);
}

int write_manifest(const char *path)
{
    return manifest_write(path);
}

void init_manifest(const char *name, const char *version, const char *main_file)
{
    manifest_clear();
    manifest_set_name(name);
    manifest_set_version(version);
    manifest_set_main(main_file);
}

int add_dep(const char *key, const char *spec)
{
    return manifest_set_dep(key, spec);
}

int run_init(void)
{
    char _qpm_name[256];
    qpm_cwd_base(_qpm_name, 256);
    manifest_clear();
    manifest_set_name(_qpm_name);
    manifest_set_version("0.1.0");
    manifest_set_main("main.qsc");
    if ((write_manifest("quill.json") != 0))
    {
        printf("%s\n", "error: failed to write quill.json");
        return 1;
    }
    printf("%s\n", "Created quill.json");
    return 0;
}

int run_add(const char *spec)
{
    if ((read_manifest("quill.json") != 0))
    {
        return 1;
    }
    if ((parse_spec_ok(spec) == false))
    {
        printf("%s\n", "error parsing spec");
        return 1;
    }
    printf("%s\n", quill_concat(quill_concat("Adding ", spec), "..."));
    if ((install(spec, "modules") != 0))
    {
        printf("%s\n", "error installing");
        return 1;
    }
    if ((add_dep(spec, spec) != 0))
    {
        printf("%s\n", "error writing dep into manifest");
        return 1;
    }
    if ((write_manifest("quill.json") != 0))
    {
        return 1;
    }
    printf("%s\n", quill_concat("Added ", spec));
    return 0;
}

int run_install(void)
{
    if ((read_manifest("quill.json") != 0))
    {
        return 1;
    }
    int n = manifest_dep_count();
    if ((n == 0))
    {
        printf("%s\n", "No dependencies to install");
        return 0;
    }

    for (int i = 0; i < manifest_dep_count(); i++)
    {
        char key[256], val[256];
        if (!manifest_dep_at(i, key, 256, val, 256))
            continue;
        printf("Installing %s...\n", key);
        if (deps_install(val, "modules") != 0)
        {
            fprintf(stderr, "error installing %s\n", key);
        }
    };
    printf("%s\n", "Done");
    return 0;
}

int run_update(void)
{
    if ((read_manifest("quill.json") != 0))
    {
        return 1;
    }

    for (int i = 0; i < manifest_dep_count(); i++)
    {
        char key[256], val[256];
        if (!manifest_dep_at(i, key, 256, val, 256))
            continue;
        printf("Updating %s...\n", key);
        if (deps_update(val, "modules") != 0)
        {
            fprintf(stderr, "error updating %s\n", key);
        }
    };
    printf("%s\n", "Done");
    return 0;
}

int run_list(void)
{
    if ((read_manifest("quill.json") != 0))
    {
        return 1;
    }
    int n = manifest_dep_count();
    if ((n == 0))
    {
        printf("%s\n", "No dependencies");
        return 0;
    }
    printf("%s\n", "Dependencies:");

    for (int i = 0; i < manifest_dep_count(); i++)
    {
        char key[256], val[256];
        if (!manifest_dep_at(i, key, 256, val, 256))
            continue;
        printf("  %s => %s\n", key, val);
    };
    return 0;
}

int run_qpm(void)
{
    int argc = qpm_argc();
    if ((argc < 2))
    {
        qpm_print_usage();
        return 1;
    }

    const char *cmd = qpm_arg(1);
    int rc = 1;
    if (strcmp(cmd, "init") == 0)
    {
        /* call back into logic: simplest is inline for init */
        char name[256];
        qpm_cwd_base(name, 256);
        manifest_clear();
        manifest_set_name(name);
        manifest_set_version("0.1.0");
        manifest_set_main("main.qsc");
        rc = manifest_write("quill.json");
        if (rc == 0)
            puts("Created quill.json");
        rc = rc == 0 ? 0 : 1;
    }
    else if (strcmp(cmd, "add") == 0)
    {
        if (qpm_argc() < 3)
        {
            fprintf(stderr, "usage: qpm add <spec>\n");
            rc = 1;
        }
        else
        {
            const char *spec = qpm_arg(2);
            if (manifest_read("quill.json") != 0)
                rc = 1;
            else if (deps_parse_ok(spec) == 0)
            {
                fprintf(stderr, "error parsing spec\n");
                rc = 1;
            }
            else
            {
                printf("Adding %s...\n", spec);
                if (deps_install(spec, "modules") != 0)
                {
                    fprintf(stderr, "error installing\n");
                    rc = 1;
                }
                else
                {
                    char full[256], as_str[320];
                    if (deps_info(spec, full, 256, as_str, 320) == 0)
                    {
                        manifest_set_dep(full, as_str);
                    }
                    else
                    {
                        manifest_set_dep(spec, spec);
                    }
                    if (manifest_write("quill.json") != 0)
                        rc = 1;
                    else
                    {
                        printf("Added %s\n", spec);
                        rc = 0;
                    }
                }
            }
        }
    }
    else if (strcmp(cmd, "install") == 0)
    {
        if (manifest_read("quill.json") != 0)
            rc = 1;
        else if (manifest_dep_count() == 0)
        {
            puts("No dependencies to install");
            rc = 0;
        }
        else
        {
            for (int i = 0; i < manifest_dep_count(); i++)
            {
                char key[256], val[256];
                if (!manifest_dep_at(i, key, 256, val, 256))
                    continue;
                printf("Installing %s...\n", key);
                if (deps_install(val, "modules") != 0)
                {
                    fprintf(stderr, "error installing %s\n", key);
                    rc = 1;
                    break;
                }
                rc = 0;
            }
            if (rc == 0)
                puts("Done");
        }
    }
    else if (strcmp(cmd, "update") == 0)
    {
        if (manifest_read("quill.json") != 0)
            rc = 1;
        else
        {
            for (int i = 0; i < manifest_dep_count(); i++)
            {
                char key[256], val[256];
                if (!manifest_dep_at(i, key, 256, val, 256))
                    continue;
                printf("Updating %s...\n", key);
                if (deps_update(val, "modules") != 0)
                    fprintf(stderr, "error updating %s\n", key);
            }
            puts("Done");
            rc = 0;
        }
    }
    else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0)
    {
        if (manifest_read("quill.json") != 0)
            rc = 1;
        else if (manifest_dep_count() == 0)
        {
            puts("No dependencies");
            rc = 0;
        }
        else
        {
            puts("Dependencies:");
            for (int i = 0; i < manifest_dep_count(); i++)
            {
                char key[256], val[256];
                if (!manifest_dep_at(i, key, 256, val, 256))
                    continue;
                printf("  %s => %s\n", key, val);
            }
            rc = 0;
        }
    }
    else
    {
        fprintf(stderr, "unknown command: %s\n", cmd);
        qpm_print_usage();
        rc = 1;
    }
    /* stash rc for Quill */
    int qpm_last_rc;
    qpm_last_rc = rc;
    ;
    qpm_last_rc = 1;
    return qpm_last_rc;
}

int main(void)
{
    return 0;
}
