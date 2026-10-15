# Compile only the native kernel, Cortex-M4F port, and heap from STM32Cube.
from os.path import join
Import("env")
framework = env.PioPlatform().get_package_dir("framework-stm32cubef4")
kernel = join(framework, "Middlewares", "Third_Party", "FreeRTOS", "Source")
env.Append(CPPPATH=[join(env.subst("$PROJECT_DIR"), "include"),
                    join(kernel, "include"), join(kernel, "portable", "GCC", "ARM_CM4F")])
env.BuildSources(join(env.subst("$BUILD_DIR"), "FreeRTOSKernel"), kernel,
                 src_filter="-<*> +<tasks.c> +<queue.c> +<list.c> +<timers.c> "
                            "+<portable/GCC/ARM_CM4F/port.c> +<portable/MemMang/heap_4.c>")
