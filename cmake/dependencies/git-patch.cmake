# In variables: patch_file, with_reset

function(patch)
    execute_process(
        COMMAND git apply ${patch_file}
        RESULT_VARIABLE ret
        ERROR_QUIET
    )
    set(ret ${ret} PARENT_SCOPE)
endfunction()

function(check_patch)
    execute_process(
        COMMAND git apply --reverse --check ${patch_file}
        RESULT_VARIABLE ret
        ERROR_QUIET
    )
    set(ret ${ret} PARENT_SCOPE)
endfunction()

# Applies the patch or checks if it has already been applied successfully previously. Will error otherwise.
function(patch_if_needed)
    patch()
    if(NOT ret EQUAL 0)
        check_patch()
    endif()
    set(ret ${ret} PARENT_SCOPE)
endfunction()

# Preserve dependency edits. An incompatible patch needs inspection, never reset.
# with_reset is accepted for compatibility with existing package declarations.
message(STATUS "Trying to apply patch ${patch_file}")
patch_if_needed()

# An additive follow-up can upgrade a previously applied version of this patch.
# Validate the complete final patch, not just the follow-up, before succeeding.
if(NOT ret EQUAL 0 AND DEFINED fallback_patch_file)
    execute_process(COMMAND git apply ${fallback_patch_file}
        RESULT_VARIABLE upgrade_ret ERROR_QUIET)
    if(upgrade_ret EQUAL 0)
        check_patch()
    endif()
endif()

if(NOT ret EQUAL 0)
    message(FATAL_ERROR "Failed to apply patch ${patch_file}; dependency files were preserved. Inspect the checkout before retrying.")
else()
    message(STATUS "Successfully patched with ${patch_file}")
endif()
