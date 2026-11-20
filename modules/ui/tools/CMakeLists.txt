# Maintainer-only Unicode table regeneration. The generated files are committed; normal builds only
# consume them, and `ctest -R unicode_generate_check` proves they are reproducible. The generators and
# their UCD inputs live beside this file so a standalone CoralUI checkout carries them.
find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
  add_custom_target(coral_regenerate_unicode
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/unicode/generate_linebreak_properties.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/unicode/generate_uax14_dfa.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/unicode/generate_bidi_properties.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/unicode/generate_bidi_brackets.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/unicode/generate_bidi_mirroring.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/unicode/generate_grapheme_properties.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/unicode/generate_hyphenation.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/unicode/generate_wordbreak_properties.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/unicode/generate_word_tries.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    COMMENT "Regenerating Unicode line-break and Bidi tables"
    VERBATIM)
endif ()
