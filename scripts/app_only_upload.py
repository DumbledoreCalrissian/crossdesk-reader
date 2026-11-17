# Upload only the application image to the stock `app` (ota_0) slot. The Picco
# keeps its vendor bootloader, partition table, NVS, otadata and safeboot
# recovery app; pio's default upload would also write the Arduino bootloader
# (0x0), partition table (0x8000) and boot_app0.bin (0xe000, inside stock NVS).
#
# The espressif32 builder copies FLASH_EXTRA_IMAGES into UPLOADERFLAGS before
# post: scripts run, so the copied offset/path pairs are removed by value here.
# The guard pins the offset from the partition table to the stock 0xE0000.
Import("env")

EXPECTED_APP_OFFSET = 0xE0000
ERASE_FLAGS = {"-e", "--erase-all", "erase-flash", "erase_flash"}

extra_pairs = [(str(o), env.subst(str(p))) for o, p in env.get("FLASH_EXTRA_IMAGES", [])]
env.Replace(FLASH_EXTRA_IMAGES=[])

if "upload" in COMMAND_LINE_TARGETS and "uploadfs" not in COMMAND_LINE_TARGETS:
    flags = list(env.get("UPLOADERFLAGS", []))
    for offset, path in extra_pairs:
        for i in range(len(flags) - 1):
            if str(flags[i]) == offset and env.subst(str(flags[i + 1])) == path:
                del flags[i : i + 2]
                break
    env.Replace(UPLOADERFLAGS=flags)


def _check_app_only(source, target, env):
    flags = [env.subst(str(f)) for f in env.get("UPLOADERFLAGS", [])]
    problems = []
    if "uploadfs" in COMMAND_LINE_TARGETS:
        # uploadfs replaces UPLOADCMD globally; firmware.bin would go to the FS offset.
        problems.append("run upload and uploadfs separately")
    stray = [f for f in flags if f.lower().startswith("0x") or f.endswith(".bin")]
    if stray:
        problems.append("extra images %r" % stray)
    erase = [f for f in flags if f in ERASE_FLAGS]
    if erase:
        problems.append("erase flags %r" % erase)
    offset = int(env.subst("$ESP32_APP_OFFSET"), 16)
    if offset != EXPECTED_APP_OFFSET:
        problems.append("app offset 0x%x, expected 0x%x" % (offset, EXPECTED_APP_OFFSET))
    if problems:
        raise SystemExit("app_only_upload: refusing to upload: " + "; ".join(problems))
    print("app_only_upload: writing only %s at 0x%x" % (source[0], offset))


env.AddPreAction("upload", _check_app_only)
