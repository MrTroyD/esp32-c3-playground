idf_component_register(SRCS "colorchord/DFT32.c" "colorchord/embeddednf.c" "colorchord/embeddedout.c" "display/bresenham.c" "display/display.c" "display/heatshrink_decoder.c" "display/palette.c" "menu2d.c" "modes/aabb_utils.c" "modes/mode_demo.c" "modes/mode_fighter.c" "modes/mode_main_menu.c" "p2pConnection.c" "swadge_esp32.c"
                    INCLUDE_DIRS "." "../components/hdw-buzzer/" "../components/hdw-qma6981/" "../components/hdw-oled/" "../components/hdw-mic/" "modes" "display" "colorchord")

function(spiffs_file_preprocessor)
    add_custom_target(spiffs_preprocessor ALL
    COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs_file_preprocessor/
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs_file_preprocessor/spiffs_file_preprocessor.exe -i ${CMAKE_CURRENT_SOURCE_DIR}/../assets/ -o ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs_image/
    )
endfunction()

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
spiffs_file_preprocessor()
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
