#ifndef QUICKJS_DEBUGGER_FILES_MANAGER_H
#define QUICKJS_DEBUGGER_FILES_MANAGER_H

#include "quickjs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* File manager for debugging - writes all executed code to disk */

typedef struct JSDebuggerFileEntry_s {
    char *filename;           /* Original filename or generated name */
    char *disk_path;          /* Path where the file was written to disk */
    char *content;            /* Content of the file */
    size_t content_length;    /* Length of the content */
    uint32_t id;              /* Unique ID for this entry */
    int is_eval;              /* True if this was from eval() */
    int is_module;            /* True if this is a module */
    struct JSDebuggerFileEntry_s *next;
} JSDebuggerFileEntry;

typedef struct JSDebuggerFileManager {
    JSDebuggerFileEntry *files;  /* Linked list of all files */
    char *debug_dir;              /* Base directory for debug files */
    uint32_t next_id;             /* Next ID to assign */
    uint32_t eval_counter;        /* Counter for eval files */
    int enabled;                  /* Whether file tracking is enabled */
    int initialized;              /* Whether the manager is initialized */
} JSDebuggerFileManager;

/* Initialize the file manager with a debug directory */
int js_debugger_files_init(JSDebuggerFileManager *manager, const char *debug_dir);

/* Clean up the file manager */
void js_debugger_files_free(JSDebuggerFileManager *manager);

/* Register a file/code that was executed
 * Returns the unique ID of the registered file, or 0 on error */
uint32_t js_debugger_files_register(JSDebuggerFileManager *manager,
                                     const char *filename,
                                     const char *content,
                                     size_t content_length,
                                     int is_eval,
                                     int is_module);

/* Get the disk path for a given original filename */
const char *js_debugger_files_get_disk_path(JSDebuggerFileManager *manager,
                                             const char *filename);

/* Get file entry by ID */
JSDebuggerFileEntry *js_debugger_files_get_by_id(JSDebuggerFileManager *manager,
                                                  uint32_t id);

/* Get file entry by original filename */
JSDebuggerFileEntry *js_debugger_files_get_by_filename(JSDebuggerFileManager *manager,
                                                        const char *filename);

/* Get file entry by content hash for eval lookups */
JSDebuggerFileEntry *js_debugger_files_get_by_content(JSDebuggerFileManager *manager,
                                                       const char *content,
                                                       size_t content_length);

/* List all registered files (for debugging purposes) */
void js_debugger_files_list(JSDebuggerFileManager *manager);

/* Enable/disable file tracking */
void js_debugger_files_set_enabled(JSDebuggerFileManager *manager, int enabled);

/* Check if file tracking is enabled */
int js_debugger_files_is_enabled(JSDebuggerFileManager *manager);

/* Clean up old debug files in the directory */
int js_debugger_files_cleanup_dir(const char *debug_dir);

/* Get or create the global file manager instance */
JSDebuggerFileManager *js_debugger_files_get_global(void);

/* Hook to be called before JS_Eval - registers the code */
uint32_t js_debugger_files_pre_eval(const char *input, size_t input_len,
                                     const char *filename, int eval_flags);

/* Hook to be called before loading a file */
uint32_t js_debugger_files_pre_load(const char *filename, 
                                     const char *content, 
                                     size_t content_length);

/* Return the debug_sources absolute path for a given original filename.
 * If the file hasn't been registered yet, this call will register it with empty content
 * to ensure a stable path is returned. Never returns NULL; falls back to original.
 */
const char *js_debugger_files_ensure_debug_path(JSDebuggerFileManager *manager,
                                                const char *original_filename,
                                                int is_module);

/* Set the current eval entry for runtime lookup */
void js_debugger_files_set_current_eval(JSDebuggerFileEntry *entry);

/* Get the current eval entry for runtime lookup */
JSDebuggerFileEntry *js_debugger_files_get_current_eval(void);




#ifdef __cplusplus
}
#endif

#endif /* QUICKJS_DEBUGGER_FILES_MANAGER_H */
