# Install script for directory: D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Syringe_pump")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "D:/pc/ESP-IDF/Env/Espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin/xtensa-esp32-elf-objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/aes.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/aria.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/asn1.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/asn1write.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/base64.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/bignum.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/block_cipher.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/build_info.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/camellia.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ccm.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/chacha20.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/chachapoly.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/check_config.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/cipher.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/cmac.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/compat-2.x.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_crypto.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_from_psa.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_from_legacy.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_superset_legacy.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/config_adjust_ssl.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/config_adjust_x509.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/config_psa.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/constant_time.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ctr_drbg.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/debug.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/des.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/dhm.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ecdh.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ecdsa.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ecjpake.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ecp.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/entropy.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/error.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/gcm.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/hkdf.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/hmac_drbg.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/lms.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/mbedtls_config.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/md.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/md5.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/net_sockets.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/nist_kw.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/oid.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/pem.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/pk.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/pkcs12.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/pkcs5.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/pkcs7.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/platform.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/platform_time.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/platform_util.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/poly1305.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/private_access.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/psa_util.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ripemd160.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/rsa.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/sha1.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/sha256.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/sha3.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/sha512.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ssl.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ssl_cache.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ssl_cookie.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/ssl_ticket.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/threading.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/timing.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/version.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/x509.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/x509_crl.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/x509_crt.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/build_info.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_adjust_auto_enabled.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_dependencies.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_key_pair_types.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_synonyms.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_builtin_composites.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_builtin_key_derivation.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_builtin_primitives.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_compat.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_config.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_driver_common.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_key_derivation.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_extra.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_legacy.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_platform.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_se_driver.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_sizes.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_struct.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_types.h"
    "D:/pc/ESP-IDF/Env/Espressif/frameworks/esp-idf-v5.5.1/components/mbedtls/mbedtls/include/psa/crypto_values.h"
    )
endif()

