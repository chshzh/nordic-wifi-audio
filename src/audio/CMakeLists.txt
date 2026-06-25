#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


FILE(GLOB audio_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        )

target_sources(app PRIVATE
        ${audio_sources}
        )

target_include_directories(app PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

if(CONFIG_SW_CODEC_OPUS) # Opus codec enabled — compile Opus + the local opus_interface glue
        # Opus source is pulled by west to <workspace>/modules/lib/opus (see west.yml).
        # Fall back to the legacy in-tree lib/opus submodule during migration; error out
        # with a clear hint if neither is present.
        if(EXISTS ${WEST_TOPDIR}/modules/lib/opus/celt)
                set(OPUS_DIR ${WEST_TOPDIR}/modules/lib/opus)
        elseif(EXISTS ${APPLICATION_SOURCE_DIR}/lib/opus/celt)
                set(OPUS_DIR ${APPLICATION_SOURCE_DIR}/lib/opus)
        else()
                message(FATAL_ERROR "Opus source not found — run 'west update' to fetch modules/lib/opus")
        endif()

        target_compile_definitions(app PUBLIC HAVE_CONFIG_H)
        target_compile_definitions(app PUBLIC ARM_MATH_CM4)
        target_compile_definitions(app PUBLIC CUSTOM_SUPPORT)
        target_compile_definitions(app PUBLIC HAVE_LRINT)

        FILE(GLOB opus_codec
                ${OPUS_DIR}/src/analysis.c
                ${OPUS_DIR}/src/extensions.c
                ${OPUS_DIR}/src/mlp.c
                ${OPUS_DIR}/src/mlp_data.c
                ${OPUS_DIR}/src/opus.c
                ${OPUS_DIR}/src/opus_decoder.c
                ${OPUS_DIR}/src/opus_encoder.c
                ${OPUS_DIR}/src/opus_multistream.c
                ${OPUS_DIR}/src/opus_multistream_decoder.c
                ${OPUS_DIR}/src/opus_multistream_encoder.c
                ${OPUS_DIR}/src/repacketizer.c
                ${OPUS_DIR}/celt/*.c
                ${OPUS_DIR}/celt/arm/arm_celt_map.c
                ${OPUS_DIR}/celt/arm/armcpu.c
                ${OPUS_DIR}/silk/*.c
                ${OPUS_DIR}/silk/fixed/*.c
                ${CMAKE_CURRENT_SOURCE_DIR}/opus_interface/*.c
                )
        list(REMOVE_ITEM opus_codec "${OPUS_DIR}/celt/opus_custom_demo.c") # exclude custom demo code

        target_sources(app PRIVATE
                ${opus_codec}
                )

        target_include_directories(app PUBLIC
                ${OPUS_DIR}
                ${OPUS_DIR}/src
                ${OPUS_DIR}/include
                ${OPUS_DIR}/celt
                ${OPUS_DIR}/celt/arm
                ${OPUS_DIR}/silk
                ${OPUS_DIR}/silk/arm
                ${OPUS_DIR}/silk/fixed
                ${CMAKE_CURRENT_SOURCE_DIR}/opus_interface
                )
endif()

