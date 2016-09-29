
try {
  // Reserve an executor from node with label arm-none-eabi-gcc
  node ("arm-none-eabi-gcc") {
    // Ensure a clean build by deleting any previous Jenkins workarea
    deleteDir()
    // Add timestamps to Jenkins console log
    timestamps {
      env.MBEDOS_ROOT = pwd()
      // Define a Jenkins stage for logging purposes
      stage ("prepare environment") {
        // Create a directory and execute steps there
        dir ("mbed-client-pal") {
          // clone the sw under test, either branch or PR depending on trigger
          checkout scm
        }
        
        dir ("mbed-os") {
          git "git@github.com:ARMmbed/mbed-os"
          execute ("git checkout tags/latest")
        }
        
        dir ("storage-volume-manager") {
          git "git@github.com:LiyouZhou/storage-volume-manager"
          execute ("sed -is \"s/\\#include \\\"mbed-trace\\/mbed_trace.h\\\"/\\#define tr_debug(fmt, ...)/\" source/storage_volume.cpp")
        }
        
        // Add mbed components
        execute ("mbed new .")
        execute ("mbed add --protocol ssh storage-abstraction")
        execute ("mbed add --protocol ssh mbed-trace")
        execute ("mbed add --protocol ssh mbed-client-libservice")

        // Execute shell command, edit file with sed
        execute ("sed -is \"s/MBED_CONF_MBED_TRACE_ENABLE || YOTTA_CFG_MBED_TRACE/defined(MBED_CONF_MBED_TRACE_ENABLE) || defined(YOTTA_CFG_MBED_TRACE)/\" mbed-trace/mbed-trace/mbed_trace.h")
        writeFile file: 'storage-abstraction/test/.mbedignore', text: '*'
        writeFile file: 'mbed-os/features/frameworks/.mbedignore', text: '*'
        writeFile file: 'storage-volume-manager/test/.mbedignore', text: '*'
      }
      
      stage ("build") {
        dir ("mbed-client-pal/Test") {
          execute ("make mbedOS_all")
        }
      }
    }
  }
} catch (error) {
    currentBuild.result = 'FAILURE'


}
