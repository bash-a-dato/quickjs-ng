#ifdef CONFIG_DEBUGGER

#include "quickjs-debugger-files-manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define PATH_SEPARATOR "\\"
/* MSVC compatibility */
#if defined(_MSC_VER)
#define strdup _strdup
#define unlink _unlink
#define getcwd _getcwd
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#endif
#else
#include <unistd.h>
#include <dirent.h>
#define PATH_SEPARATOR "/"
#endif

/* Global instance of the file manager */
static JSDebuggerFileManager *g_file_manager = NULL;

/* Helper function to create directories recursively */
static int create_directory_recursive(const char *path) {
    char *path_copy = strdup(path);
    char *p = path_copy;
    
    if (!path_copy) {
        return -1;
    }
    
    /* Skip drive letter on Windows */
#ifdef _WIN32
    if (p[1] == ':') {
        p += 2;
    }
#endif
    
    /* Skip initial separator */
    if (*p == '/' || *p == '\\') {
        p++;
    }
    
    while (*p) {
        /* Find next separator */
        while (*p && *p != '/' && *p != '\\') {
            p++;
        }
        
        if (*p) {
            char sep = *p;
            *p = '\0';
            
            /* Try to create directory */
            if (mkdir(path_copy, 0755) != 0) {
                if (errno != EEXIST) {
                    free(path_copy);
                    return -1;
                }
            }
            
            *p = sep;
            p++;
        }
    }
    
    /* Create final directory */
    if (mkdir(path_copy, 0755) != 0) {
        if (errno != EEXIST) {
            free(path_copy);
            return -1;
        }
    }
    
    free(path_copy);
    return 0;
}

/* Helper function to sanitize filename for use as disk path */
static char *sanitize_filename(const char *filename) {
    if (!filename) {
        return NULL;
    }
    
    size_t len = strlen(filename);
    char *sanitized = malloc(len + 1);
    if (!sanitized) {
        return NULL;
    }
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = filename[i];
        
        /* Replace problematic characters */
        if (c == '<' || c == '>' || c == ':' || c == '"' || 
            c == '|' || c == '?' || c == '*') {
            sanitized[j++] = '_';
        } else if (c == '/' || c == '\\') {
            /* Convert path separators to underscores for flattening */
            sanitized[j++] = '_';
        } else {
            sanitized[j++] = c;
        }
    }
    
    sanitized[j] = '\0';
    
    /* Remove existing .js/.mjs extensions to avoid double extensions */
    if (j >= 3 && strcmp(&sanitized[j-3], ".js") == 0) {
        sanitized[j-3] = '\0';
    } else if (j >= 4 && strcmp(&sanitized[j-4], ".mjs") == 0) {
        sanitized[j-4] = '\0';
    }
    
    return sanitized;
}

/* Helper function to write content to file */
static int write_file(const char *path, const char *content, size_t length) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        return -1;
    }
    
    size_t written = fwrite(content, 1, length, fp);
    fclose(fp);
    
    return (written == length) ? 0 : -1;
}

/* Initialize the file manager */
int js_debugger_files_init(JSDebuggerFileManager *manager, const char *debug_dir) {
    if (!manager) {
        return -1;
    }
    
    memset(manager, 0, sizeof(JSDebuggerFileManager));
    
    /* Set up debug directory */
    if (debug_dir) {
        manager->debug_dir = strdup(debug_dir);
    } else {
        /* Default to ./debug_sources */
        manager->debug_dir = strdup("debug_sources");
    }
    
    if (!manager->debug_dir) {
        return -1;
    }
    
    /* Create the debug directory */
    if (create_directory_recursive(manager->debug_dir) != 0) {
        free(manager->debug_dir);
        manager->debug_dir = NULL;
        return -1;
    }
    
    manager->next_id = 1;
    manager->eval_counter = 1;
    manager->enabled = 1;
    manager->initialized = 1;
    
    return 0;
}

/* Clean up the file manager */
void js_debugger_files_free(JSDebuggerFileManager *manager) {
    if (!manager) {
        return;
    }
    
    /* Free all file entries */
    JSDebuggerFileEntry *entry = manager->files;
    while (entry) {
        JSDebuggerFileEntry *next = entry->next;
        
        if (entry->filename) {
            free(entry->filename);
        }
        if (entry->disk_path) {
            free(entry->disk_path);
        }
        if (entry->content) {
            free(entry->content);
        }
        
        free(entry);
        entry = next;
    }
    
    if (manager->debug_dir) {
        free(manager->debug_dir);
    }
    
    memset(manager, 0, sizeof(JSDebuggerFileManager));
}

/* Register a file/code that was executed */
uint32_t js_debugger_files_register(JSDebuggerFileManager *manager,
                                     const char *filename,
                                     const char *content,
                                     size_t content_length,
                                     int is_eval,
                                     int is_module) {
    if (!manager || !manager->initialized || !manager->enabled) {
        return 0;
    }
    
    if (!content || content_length == 0) {
        return 0;
    }
    
    /* Check if we already have this file registered */
    if (filename && !is_eval) {
        JSDebuggerFileEntry *existing = js_debugger_files_get_by_filename(manager, filename);
        if (existing) {
            /* Update content if different */
            if (existing->content_length != content_length ||
                memcmp(existing->content, content, content_length) != 0) {
                
                /* Update content */
                char *new_content = malloc(content_length + 1);
                if (new_content) {
                    memcpy(new_content, content, content_length);
                    new_content[content_length] = '\0';
                    free(existing->content);
                    existing->content = new_content;
                    existing->content_length = content_length;
                    
                    /* Rewrite file to disk */
                    write_file(existing->disk_path, content, content_length);
                }
            }
            return existing->id;
        }
    }
    
    /* Create new entry */
    JSDebuggerFileEntry *entry = calloc(1, sizeof(JSDebuggerFileEntry));
    if (!entry) {
        return 0;
    }
    
    entry->id = manager->next_id++;
    entry->is_eval = is_eval;
    entry->is_module = is_module;
    
    /* Set filename */
    if (is_eval) {
        /* Generate Chrome-like VM filename for eval code based on content hash */
        uint32_t hash = 0;
        for (size_t i = 0; i < content_length; i++) {
            hash = hash * 31 + (unsigned char)content[i];
        }
        
        /* Check if we already have this eval content */
        JSDebuggerFileEntry *existing = manager->files;
        while (existing) {
            if (existing->is_eval && existing->content_length == content_length &&
                memcmp(existing->content, content, content_length) == 0) {
                /* Same eval content - reuse the existing filename */
                entry->filename = strdup(existing->filename);
                break;
            }
            existing = existing->next;
        }
        
        if (!entry->filename) {
            /* New eval content - generate VM#### filename */
            char vm_name[256];
            snprintf(vm_name, sizeof(vm_name), "VM%u", hash);
            entry->filename = strdup(vm_name);
        }
    } else if (filename) {
        entry->filename = strdup(filename);
    } else {
        entry->filename = strdup("anonymous");
    }
    
    if (!entry->filename) {
        free(entry);
        return 0;
    }
    
    /* Copy content */
    entry->content = malloc(content_length + 1);
    if (!entry->content) {
        free(entry->filename);
        free(entry);
        return 0;
    }
    memcpy(entry->content, content, content_length);
    entry->content[content_length] = '\0';
    entry->content_length = content_length;
    
    /* Generate disk path */
    char *sanitized = sanitize_filename(entry->filename);
    if (!sanitized) {
        free(entry->content);
        free(entry->filename);
        free(entry);
        return 0;
    }
    
    /* Build full path - use larger buffer for absolute paths */
    size_t path_len = 4096; /* Large enough for absolute paths */
    entry->disk_path = malloc(path_len);
    if (!entry->disk_path) {
        free(sanitized);
        free(entry->content);
        free(entry->filename);
        free(entry);
        return 0;
    }
    
    /* Create consistent filename without ID prefix */
    const char *extension = is_module ? ".mjs" : ".js";
    
    /* Check if debug_dir is already absolute */
    int is_absolute = 0;
#ifdef _WIN32
    is_absolute = (strlen(manager->debug_dir) >= 2 && manager->debug_dir[1] == ':') ||
                  (manager->debug_dir[0] == '\\' && manager->debug_dir[1] == '\\');
#else
    is_absolute = (manager->debug_dir[0] == '/');
#endif
    
    if (is_absolute) {
        if (is_eval) {
            /* Put eval files in debug_sources/evals/ subfolder */
            snprintf(entry->disk_path, path_len, "%s%sevals%s%s%s",
                     manager->debug_dir, PATH_SEPARATOR, PATH_SEPARATOR, sanitized, extension);
        } else {
            snprintf(entry->disk_path, path_len, "%s%s%s%s",
                     manager->debug_dir, PATH_SEPARATOR, sanitized, extension);
        }
    } else {
        /* Convert to absolute path */
        char cwd[4096];
#ifdef _WIN32
        _getcwd(cwd, sizeof(cwd));
#else
        getcwd(cwd, sizeof(cwd));
#endif
        if (is_eval) {
            /* Put eval files in debug_sources/evals/ subfolder */
            snprintf(entry->disk_path, path_len, "%s%s%s%sevals%s%s%s",
                     cwd, PATH_SEPARATOR, manager->debug_dir, PATH_SEPARATOR, PATH_SEPARATOR, sanitized, extension);
        } else {
            snprintf(entry->disk_path, path_len, "%s%s%s%s%s%s",
                     cwd, PATH_SEPARATOR, manager->debug_dir, PATH_SEPARATOR, sanitized, extension);
        }
    }
    
    free(sanitized);
    
    /* Create evals subdirectory if needed */
    if (is_eval) {
        char evals_dir[4096];
        if (is_absolute) {
            snprintf(evals_dir, sizeof(evals_dir), "%s%sevals", manager->debug_dir, PATH_SEPARATOR);
        } else {
            char cwd[4096];
#ifdef _WIN32
            _getcwd(cwd, sizeof(cwd));
#else
            getcwd(cwd, sizeof(cwd));
#endif
            snprintf(evals_dir, sizeof(evals_dir), "%s%s%s%sevals", 
                     cwd, PATH_SEPARATOR, manager->debug_dir, PATH_SEPARATOR);
        }
        create_directory_recursive(evals_dir);
    }
    
    /* Write to disk */
    if (write_file(entry->disk_path, content, content_length) != 0) {
        fprintf(stderr, "Failed to write debug file: %s\n", entry->disk_path);
        /* Continue anyway - we still track it in memory */
    }
    
    /* Add to linked list */
    entry->next = manager->files;
    manager->files = entry;
    
    /* Set as current eval entry for runtime lookup */
    if (is_eval) {
        js_debugger_files_set_current_eval(entry);
    }
    
    /* Log the registration */
    printf("[DEBUG] Registered file: %s -> %s (%s%s)\n",
           entry->filename, entry->disk_path,
           is_eval ? "eval, " : "",
           is_module ? "module" : "script");
    
    return entry->id;
}

/* Get the disk path for a given original filename */
const char *js_debugger_files_get_disk_path(JSDebuggerFileManager *manager,
                                             const char *filename) {
    if (!manager || !filename) {
        return NULL;
    }
    
    JSDebuggerFileEntry *entry = js_debugger_files_get_by_filename(manager, filename);
    return entry ? entry->disk_path : NULL;
}

/* Get file entry by ID */
JSDebuggerFileEntry *js_debugger_files_get_by_id(JSDebuggerFileManager *manager,
                                                  uint32_t id) {
    if (!manager || id == 0) {
        return NULL;
    }
    
    JSDebuggerFileEntry *entry = manager->files;
    while (entry) {
        if (entry->id == id) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/* Get file entry by original filename */
JSDebuggerFileEntry *js_debugger_files_get_by_filename(JSDebuggerFileManager *manager,
                                                        const char *filename) {
    if (!manager || !filename) {
        return NULL;
    }
    
    JSDebuggerFileEntry *entry = manager->files;
    while (entry) {
        if (entry->filename && strcmp(entry->filename, filename) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/* Get file entry by content hash for eval lookups */
JSDebuggerFileEntry *js_debugger_files_get_by_content(JSDebuggerFileManager *manager,
                                                       const char *content,
                                                       size_t content_length) {
    if (!manager || !content) {
        return NULL;
    }
    
    JSDebuggerFileEntry *entry = manager->files;
    while (entry) {
        if (entry->is_eval && 
            entry->content_length == content_length &&
            entry->content &&
            memcmp(entry->content, content, content_length) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/* List all registered files */
void js_debugger_files_list(JSDebuggerFileManager *manager) {
    if (!manager) {
        return;
    }
    
    printf("=== Registered Debug Files ===\n");
    printf("Debug directory: %s\n", manager->debug_dir);
    printf("Files tracked: %s\n", manager->enabled ? "enabled" : "disabled");
    printf("\n");
    
    JSDebuggerFileEntry *entry = manager->files;
    while (entry) {
        printf("ID %04u: %s\n", entry->id, entry->filename);
        printf("  Disk: %s\n", entry->disk_path);
        printf("  Type: %s%s\n",
               entry->is_eval ? "eval " : "",
               entry->is_module ? "module" : "script");
        printf("  Size: %zu bytes\n", entry->content_length);
        printf("\n");
        entry = entry->next;
    }
    
    if (!manager->files) {
        printf("No files registered yet.\n");
    }
}

/* Enable/disable file tracking */
void js_debugger_files_set_enabled(JSDebuggerFileManager *manager, int enabled) {
    if (manager) {
        manager->enabled = enabled;
    }
}

/* Check if file tracking is enabled */
int js_debugger_files_is_enabled(JSDebuggerFileManager *manager) {
    return manager && manager->enabled;
}

/* Clean up old debug files in the directory */
int js_debugger_files_cleanup_dir(const char *debug_dir) {
    if (!debug_dir) {
        return -1;
    }
    
#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", debug_dir);
    
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(pattern, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return -1;
    }
    
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char filepath[MAX_PATH];
            snprintf(filepath, sizeof(filepath), "%s\\%s", debug_dir, findData.cFileName);
            DeleteFile(filepath);
        }
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
#else
    DIR *dir = opendir(debug_dir);
    if (!dir) {
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s", debug_dir, entry->d_name);
            unlink(filepath);
        }
    }
    
    closedir(dir);
#endif
    
    return 0;
}

/* Get or create the global file manager instance */
JSDebuggerFileManager *js_debugger_files_get_global(void) {
    if (!g_file_manager) {
        g_file_manager = calloc(1, sizeof(JSDebuggerFileManager));
        if (g_file_manager) {
            if (js_debugger_files_init(g_file_manager, NULL) != 0) {
                free(g_file_manager);
                g_file_manager = NULL;
            }
        }
    }
    return g_file_manager;
}

/* Hook to be called before JS_Eval */
uint32_t js_debugger_files_pre_eval(const char *input, size_t input_len,
                                     const char *filename, int eval_flags) {
    JSDebuggerFileManager *manager = js_debugger_files_get_global();
    if (!manager) {
        return 0;
    }
    
    int is_module = (eval_flags & 0x03) == 1; /* JS_EVAL_TYPE_MODULE */
    int is_eval = !filename || 
                   strstr(filename, "<eval") != NULL ||
                   strstr(filename, "<cmdline>") != NULL ||
                   strcmp(filename, "<evalScript>") == 0 ||
                   strcmp(filename, "<input>") == 0;
    
    return js_debugger_files_register(manager, filename, input, input_len,
                                       is_eval, is_module);
}

/* Hook to be called before loading a file */
uint32_t js_debugger_files_pre_load(const char *filename, 
                                     const char *content, 
                                     size_t content_length) {
    JSDebuggerFileManager *manager = js_debugger_files_get_global();
    if (!manager) {
        return 0;
    }
    
    return js_debugger_files_register(manager, filename, content, content_length,
                                       0, 0);
}

/* Return or create the debug path for a given original filename */
const char *js_debugger_files_ensure_debug_path(JSDebuggerFileManager *manager,
                                                const char *original_filename,
                                                int is_module) {
    if (!manager || !original_filename)
        return original_filename;

    JSDebuggerFileEntry *existing = js_debugger_files_get_by_filename(manager, original_filename);
    if (existing && existing->disk_path)
        return existing->disk_path;

    /* Register with empty content to establish a path if needed */
    (void)js_debugger_files_register(manager, original_filename, "", 0,
                                     0, is_module);
    existing = js_debugger_files_get_by_filename(manager, original_filename);
    if (existing && existing->disk_path)
        return existing->disk_path;

    return original_filename;
}

/* Global variable to store the last eval entry for runtime lookup */
static JSDebuggerFileEntry *g_last_eval_entry = NULL;

/* Set the current eval entry for runtime lookup */
void js_debugger_files_set_current_eval(JSDebuggerFileEntry *entry) {
    g_last_eval_entry = entry;
}

/* Get the current eval entry for runtime lookup */
JSDebuggerFileEntry *js_debugger_files_get_current_eval(void) {
    return g_last_eval_entry;
}

int js_debugger_files_logf(const char *fmt, ...)
{
    JSDebuggerFileManager *manager = js_debugger_files_get_global();
    if (!manager || !manager->initialized || !manager->enabled)
        return -1;

    /* Build logs path: <debug_dir>/logs.txt (make absolute if needed) */
    char path[4096];
    int is_absolute = 0;
#ifdef _WIN32
    is_absolute = (strlen(manager->debug_dir) >= 2 && manager->debug_dir[1] == ':') ||
                  (manager->debug_dir[0] == '\\' && manager->debug_dir[1] == '\\');
#else
    is_absolute = (manager->debug_dir[0] == '/');
#endif
    if (is_absolute) {
        snprintf(path, sizeof(path), "%s%slogs.txt", manager->debug_dir, PATH_SEPARATOR);
    } else {
        char cwd[4096];
#ifdef _WIN32
        _getcwd(cwd, sizeof(cwd));
#else
        getcwd(cwd, sizeof(cwd));
#endif
        snprintf(path, sizeof(path), "%s%s%s%slogs.txt",
                 cwd, PATH_SEPARATOR, manager->debug_dir, PATH_SEPARATOR);
    }

    FILE *fp = fopen(path, "ab");
    if (!fp)
        return -1;

    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fflush(fp);
    fclose(fp);
    return 0;
}

#endif /* CONFIG_DEBUGGER */
