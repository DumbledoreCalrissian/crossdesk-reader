"""Source package references must match the installed dependency version."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location(
    "release_license", Path(__file__).resolve().parents[1] / "scripts/build_release_license.py"
)
release_license = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release_license)


class SourceReferenceTest(unittest.TestCase):
    def test_registry_source_uses_installed_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            (path / ".piopm").write_text(json.dumps({
                "name": "Arduino-wolfSSL", "version": "5.7.2",
                "spec": {"owner": "wolfssl", "uri": None},
            }))
            self.assertEqual(
                release_license.source_reference(path),
                "https://dl.registry.platformio.org/download/wolfssl/library/"
                "Arduino-wolfSSL/5.7.2/Arduino-wolfSSL-5.7.2.tar.gz",
            )

    def test_framework_source_preserves_exact_archive_url(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            source_url = "https://example.org/core-3.3.11.tar.xz"
            (path / ".piopm").write_text(json.dumps({"spec": {"uri": source_url}}))
            self.assertEqual(release_license.source_reference(path), source_url)

    def test_missing_metadata_does_not_silently_omit_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "Missing source package metadata"):
                release_license.source_reference(Path(temporary))

    def test_sdk_mismatch_does_not_label_unrelated_sources_as_corresponding(self):
        with patch.object(release_license, "git", side_effect=["reader-sha", "pinned-sdk", "other-sdk"]):
            with self.assertRaisesRegex(ValueError, "SDK checkout differs"):
                release_license.build_license("default", "owner/reader", Path("packages"))

    def test_sdk_link_uses_the_submodule_repository(self):
        for url in ("https://github.com/owner/sdk.git", "git@github.com:owner/sdk.git"):
            with patch.object(release_license, "git", return_value=url):
                self.assertEqual(release_license.sdk_repository_url(), "https://github.com/owner/sdk")

    def test_unknown_sdk_url_form_is_rejected(self):
        with patch.object(release_license, "git", return_value="ssh://git@github.com/owner/sdk.git"):
            with self.assertRaisesRegex(ValueError, "SDK submodule URL"):
                release_license.sdk_repository_url()


if __name__ == "__main__":
    unittest.main()
