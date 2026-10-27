# Releasing the Android app

## Once: create the release key

```powershell
.\Scripts\CreateReleaseKey.ps1
```

It makes `D:\GFCKeys\gfc-release.keystore` (alias `gfc`). keytool asks you for the password, which never
goes into the project. Every update of the app must be signed with this same key:

- Back up the keystore file now (e.g. `F:\GFC\`) and keep its password apart from it.
- If it's lost, installed copies can't be updated: testers would uninstall and reinstall.

The script prints the key's SHA-256 fingerprint. Put it on the node so links to GFC posts open the app (App Links):

```powershell
ssh -t gfc-node gfc config
```

and set `GFC_ANDROID_PACKAGE=tech.galacticfederation.gfc` and `GFC_ANDROID_CERT_SHA256=<fingerprint>`.

## Each release

1. Bump `StoreVersion` (by one, every upload) and `VersionDisplayName` in `Config/DefaultEngine.ini`.
2. Build:

   ```powershell
   .\Scripts\PackageAndroid.ps1 -Release           # signed .apk for testers
   .\Scripts\PackageAndroid.ps1 -Release -Bundle   # signed .aab for Google Play
   ```

   It asks for the key's password, builds Shipping (no debug overlay or console), and signs with the release
   key. The key and password reach Unreal only through `Build/Android/gfc-release.keystore` and
   `Config/Android/AndroidEngine.ini`, which are git-ignored and deleted when the build ends.
3. The result is in `Packaged\Release\` as `GFC-<version>-<commit>.apk` (or `.aab`).

## Getting it to testers

**Sideloading the .apk** (quickest). Testers allow installs from their browser or file app, then open the file.
Android shows an "unknown app" warning, and Play Protect may add "unrecognised developer". That is normal for
any app outside a store; signing with your key doesn't remove it.

**Google Play internal testing** (no warnings, automatic updates, up to 100 testers):

1. Create a Google Play Console developer account (a one-off fee; identity checks can take days).
2. Create the app with package name `tech.galacticfederation.gfc`.
3. Turn on Play App Signing and upload `.aab` builds signed with your release key. Google keeps its own
   signing key for the store copy; your key becomes the upload key, so a lost upload key can be reset.
4. Testing > Internal testing: add testers' emails, upload the `.aab`, share the opt-in link.
5. With Play App Signing, Play Console shows Google's app signing certificate fingerprint. Add it to
   `GFC_ANDROID_CERT_SHA256` next to yours (comma-separated) so App Links work for Play installs too.

Play also needs a privacy policy URL (the app collects phone numbers for verification) and the Data safety form.

## Development builds

`.\Scripts\PackageAndroid.ps1` alone still makes the Development build, signed with the debug key, with the debug
overlay. Use it for your own phone, not for testers.
