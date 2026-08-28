#include <stdio.h>
#include <string.h>

extern void qpm_set_args(int, char **);
extern void qpm_print_usage(void);
extern void qpm_cwd_base(char *, int);

extern int  manifest_read(const char *);
extern int  manifest_write(const char *);
extern void manifest_clear(void);
extern void manifest_set_name(const char *);
extern void manifest_set_version(const char *);
extern void manifest_set_main(const char *);
extern int  manifest_set_dep(const char *, const char *);
extern int  manifest_dep_count(void);
extern int  manifest_dep_at(int, char *, int, char *, int);

extern int  deps_parse_ok(const char *);
extern int  deps_install(const char *, const char *);
extern int  deps_update(const char *, const char *);
extern int  deps_info(const char *, char *, int, char *, int);

int main(int argc, char **argv) {
    qpm_set_args(argc, argv);

    if (argc < 2) {
        qpm_print_usage();
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "init") == 0) {
        char name[256];
        qpm_cwd_base(name, 256);
        manifest_clear();
        manifest_set_name(name);
        manifest_set_version("0.1.0");
        manifest_set_main("main.qsc");
        if (manifest_write("quill.json") != 0)
            return 1;
        puts("Created quill.json");
        return 0;
    }

    if (strcmp(cmd, "add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "usage: qpm add <spec>\n");
            return 1;
        }
        const char *spec = argv[2];
        if (manifest_read("quill.json") != 0)
            return 1;
        if (!deps_parse_ok(spec)) {
            fprintf(stderr, "error parsing spec\n");
            return 1;
        }
        printf("Adding %s...\n", spec);
        if (deps_install(spec, "modules") != 0)
            return 1;
        char full[256], as_str[320];
        if (deps_info(spec, full, 256, as_str, 320) == 0)
            manifest_set_dep(full, as_str);
        else
            manifest_set_dep(spec, spec);
        if (manifest_write("quill.json") != 0)
            return 1;
        printf("Added %s\n", spec);
        return 0;
    }

    if (strcmp(cmd, "install") == 0) {
        if (manifest_read("quill.json") != 0)
            return 1;
        if (manifest_dep_count() == 0) {
            puts("No dependencies to install");
            return 0;
        }
        for (int i = 0; i < manifest_dep_count(); i++) {
            char key[256], val[256];
            if (!manifest_dep_at(i, key, 256, val, 256))
                continue;
            printf("Installing %s...\n", key);
            if (deps_install(val, "modules") != 0)
                return 1;
        }
        puts("Done");
        return 0;
    }

    if (strcmp(cmd, "update") == 0) {
        if (manifest_read("quill.json") != 0)
            return 1;
        for (int i = 0; i < manifest_dep_count(); i++) {
            char key[256], val[256];
            if (!manifest_dep_at(i, key, 256, val, 256))
                continue;
            printf("Updating %s...\n", key);
            deps_update(val, "modules");
        }
        puts("Done");
        return 0;
    }

    if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0) {
        if (manifest_read("quill.json") != 0)
            return 1;
        if (manifest_dep_count() == 0) {
            puts("No dependencies");
            return 0;
        }
        puts("Dependencies:");
        for (int i = 0; i < manifest_dep_count(); i++) {
            char key[256], val[256];
            if (!manifest_dep_at(i, key, 256, val, 256))
                continue;
            printf("  %s => %s\n", key, val);
        }
        return 0;
    }

    fprintf(stderr, "unknown command: %s\n", cmd);
    qpm_print_usage();
    return 1;
}