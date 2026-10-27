# GFC Client roadmap

The first GFC build ships on **Android phones**. Windows is the development and
test loop, but every choice is made for a mid-range phone (3 to 4 GB RAM).
Unlike X and similar apps, GFC shows **3D content** in the app.

## Mobile budgets

These are the client's limits. The node should enforce matching limits on
upload, so creators learn about them before their seed fails on phones.

| Budget | Target | Where |
|---|---|---|
| Frame rate | 60 fps UI, 30 fps 3D viewer | Mobile renderer; the viewer renders only when the camera moves |
| Base APK | under ~150 MB | Unused plugins off (`GFCClient.uproject`), engine content only, no Starter Content |
| RAM | stays well under 1 GB on a 3 to 4 GB phone | One live heavy item; bounded thumbnails |
| Model file | 50 MB | `MaxModelMB` |
| Triangles per model | 300,000 | `MaxModelTriangles`; checked before vertices are read |
| Texture size | 2048 px (bigger ones are downscaled) | `MaxTextureSize` |
| Images per model | 16 | `FGFCModelBudget::MaxImages` |
| Model load time | 45 s, download plus build | `ModelLoadTimeoutSeconds` |
| Full image file | 20 MB | `MaxImageMB` |
| Disk cache | 200 MB total, session-only | `MaxDiskCacheMB`; see [cache-policy.md](cache-policy.md) |
| Mobile data | ask above 10 MB | `CellularConfirmMB` |

**Suggested backend limits** (GFCCoin `/seeds/upload`): reject `.glb` over
50 MB or over 300k triangles; reject Draco/meshopt/Basis-compressed glTF
until the client supports them; reject `.gltf` with external URIs; require a
thumbnail for 3D seeds; cap images at 20 MB.

Content lifecycle and its baseline check: [cache-policy.md](cache-policy.md).

### Rendering settings (`Config/DefaultEngine.ini`)

- Mobile forward shading (`r.Mobile.ShadingPath=0`), MSAA 4x; Vulkan with an OpenGL ES 3.1 fallback; ARM64 only.
- Off: Lumen (`r.DynamicGlobalIlluminationMethod=0`, `r.ReflectionMethod=0`), Nanite, virtual shadow maps,
  ray tracing, distance fields, static lighting, virtual textures, Substrate, sky atmosphere, auto exposure,
  bloom, motion blur, ambient occlusion, lens flares.
- PC targets `PCD3D_ES31` too: `Scripts\Run.ps1` runs at `-featureleveles31` in a portrait 432x900 window,
  which matches what phones render. In the editor: **Settings > Preview Platform > Android Vulkan** (or ES 3.1).
- UI scales by the shortest screen side in dp: 412 dp wide = scale 1.0.

## Milestone 1 (this PR)

- C++ project; the `GFCApi` module (HTTP + Json) is Blueprint-callable with Bearer auth; the session is DPAPI-protected on PC.
- Portrait, touch-first UI built in C++ UMG: login/register, feed, seed detail, wallet with send, history and the backup prompt.
- **In-app 3D viewer**: runtime `.glb` / `.gltf` (embedded) with orbit, pinch-zoom and pan, and budget checks.
- Full-size images and video/audio playback, all on tap; thumbnails first.
- Cache policy with one owner, delete-on-scroll-away, and the triggers above.
- Qualified views reported to `/seeds/{id}/played` (below).
- Deep links: Android App Links for `https://node.galacticfederation.tech/p/{id}` and `gfc://seed/{id}`.

### Runtime glTF loader: decision

| Option | Packaged builds | Android | License | Notes |
|---|---|---|---|---|
| **Own minimal loader** (`FGFCModelParser` + `UProceduralMeshComponent`): **chosen for M1** | Yes | Yes (no platform code) | Ours | Static meshes, node transforms, metallic-roughness + base colour texture. Every offset bounds-checked; budgets enforced before allocation. No third-party download. |
| glTFRuntime (rdeioris) | Yes | Reported working | MIT | Most complete (skeletal meshes, animations, morph targets). The best upgrade path if skinned or animated models are needed. Check for a UE 5.7 release and test on a device first. |
| Epic Interchange runtime import | Partly | Unclear | Engine | A UE 5.7.4 issue report (UE-377300) describes broken meshes, materials and textures from runtime import in Shipping builds. Not used. |

Not supported yet, and rejected with a clear message: Draco/meshopt
compression, KHR_texture_basisu, sparse accessors, external URIs. Skins and
animations are ignored (the bind pose is shown). Normal, occlusion and
emissive maps are ignored. The viewer is behind `AGFCModelViewer`, so the loader
can be swapped (for glTFRuntime, say) without UI changes.

### Qualified views and creator rewards

The node pays the creator (`POST /seeds/{id}/played`): once per seed and
viewer, at most once per viewer per creator per day, and only for
phone-verified viewers whose accounts are at least 24 h old. The client's job
is to report a **real** view, once. `FGFCViewTracker` decides, with thresholds
in Project Settings > GFC API > Views:

| Seed | Qualified when |
|---|---|
| 3D | the model is loaded and on screen for 3 s (`ModelViewSeconds`), or the user orbits, zooms or pans |
| Video / audio | 5 s played (`MediaPlaySeconds`), or 50% (`ShortClipFraction`) of a clip under 10 s (`ShortClipSeconds`) |
| Image | the full-size image on screen for 2 s (`ImageViewSeconds`) |

- On-screen time must be continuous: a card flashing past in a fast scroll never qualifies (tested in `GFC.Mobile.QualifiedView`).
- At most once per seed per app session (an in-memory set); re-views after scroll-back don't call again.
- Not for the user's own seeds, and not when logged out. Uses the Bearer token.
- `credited: false` reasons (daily limit, unverified phone and so on) are normal, logged, never shown as errors.
- If offline or the call fails, the report is dropped, not queued to disk.

### Deep links (share on X, open in the app)

- Shared URL: `https://node.galacticfederation.tech/p/{id}` (`ShareBaseUrl`). The share page's
  "Open in GFC" button uses `intent://` → `gfc://seed/{id}` (backend work in progress).
- Android: `Source/GFCApi/GFCApi_UPL.xml` adds an `autoVerify` intent filter for that host and `/p/`,
  plus `gfc://seed/*`. Links reach C++ on cold start and through `onNewIntent` (`singleTask`).
- Parsing: `FGFCDeepLink` (tested in `GFC.Mobile.DeepLink`). Only exact hosts, https, one id segment of
  `[A-Za-z0-9_-]`, max 128 characters. Anything else is ignored.
- Logged out: the seed opens anyway (seeds are public); "Log in" returns to it afterwards.
- PC: pass the link on the command line (`.\Scripts\Run.ps1 -Link "gfc://seed/<id>"`).
- The package name is `tech.galacticfederation.gfc`, set once in `DefaultEngine.ini`. It's permanent once on Google Play.
- Signing fingerprints and adb checks: see README > Android > App Links.

## Milestone 1b: on the phone (debugging, posting, feed fairness)

Built in this PR, and verified on PC as far as PC allows:

- **Development builds by default** for the phone, with Verbose GFC logs, `stat fps`/`stat unit`, the debug
  overlay (three-finger tap or `gfc.debug`: fps, API calls and errors, cache, live 3D memory), and crash and
  ensure output in the log. The README has the USB debugging, `adb install -r` and `adb logcat` steps.
- **Posting**: **+ Post** offers take a photo, record a video, or choose from the gallery. These use the system
  camera intents writing into MediaStore and the system photo picker, through `GFCApi_UPL.xml`. The upload to
  `/seeds/upload` is streamed, with progress, cancel, a 200 MB check, retries, title and description, and a
  device-made thumbnail. `repost_of` in the response shows "This looks like a repost of @original. They'll get
  the credit." Feed cards show reposts too.
- **Feed fairness**: no autoplay; video plays inline, muted, on tap; at most two posts in a row per creator.
  See the principles in `docs/ui-spec.md`.

Still to verify or build on a real phone:

- The camera, video and picker Java bridge hasn't been compiled or run (no Android toolchain yet). Check on
  Android 9 (the storage permission path), Android 10 to 12 (`ACTION_GET_CONTENT`) and Android 13+ (the photo picker).
- If Android kills the app while the camera is open, the capture is in the gallery but the post is lost.
  Recover by re-picking it from the gallery for now; later, persist a tiny "pending capture" marker.
- **Video compression before upload**: phone videos run 50 to 200 MB a minute. Add on-device transcoding
  (Android `MediaCodec` / Media3 Transformer, e.g. 1080p at about 8 Mbit/s) before posting, and show the
  estimated size.
- Uploads in the background (a foreground service) so posting survives the screen turning off.
- Icons: add an open-licensed set (Material Symbols, Apache 2.0, or Lucide, ISC); text labels for now.

## Android build (next)

- Install Android support in the Epic launcher (UE 5.7 > Options > Android) and the SDK/NDK (README).
- Store the token in the **Android Keystore** (`TODO(android-keystore)` in `GFCSessionStore.h`).
- Wallet backup export: hand the file to the share sheet / Storage Access Framework (`TODO(android)`).
- Verify on a real device: the App Links verification, the cache folder (`getCacheDir`), the media player,
  scene-capture performance at 30 fps, and APK size.
- Target SDK: UE 5.7's default is 34. Google Play requires newer targets over time; raise it when packaging for Play.

## Milestone 2: Echo Hunt

- 3D audio: spatialised emitters (MetaSounds plus a spatialisation plugin; check HRTF support on Android).
- Gyro: device motion to aim the listener (Enhanced Input / motion events). PC fallback: the mouse.
- Rounds from `POST /echohunt/start`, answers to `POST /echohunt/submit` (see `docs/echo-hunt.md` in GFCCoin).
- **Play Integrity**: a UPL addition adding `com.google.android.play:integrity` and a Java helper
  (`StandardIntegrityManager`: prepare once, then `request()` with the node's request hash), called over JNI,
  with the token returned through a `JNI_METHOD` callback and sent with the submission. The node needs
  `GFC_PLAY_INTEGRITY_SA_FILE` and `GFC_ANDROID_PACKAGE` (not configured yet). Never faked on PC.
- Viewer upgrades: skinned and animated models (glTFRuntime or extend the parser), and normal/emissive maps.

## Milestone 3: phone verification, uploads, tips

- **Phone verification** (`/verify/phone/start` and `/check`). Public nodes require a Cloudflare Turnstile
  token: show the widget in a `UWebBrowser` (system WebView on Android) on a small page that hands the token
  back. Verification unlocks the 100 GFC grant and rewarded plays.
- **Uploads** (`/seeds/upload`): a picker via the Storage Access Framework on Android; generate a JPG
  thumbnail client-side (render the model in the viewer for 3D seeds); check the budgets above before uploading.
- **Tips**: "Tip creator" on the seed detail, sending to `reward_wallet`.
- Likes, comments, bookmarks (plus the "keep offline" opt-in), profiles, search.
- **Scoped tokens**: after login, mint a limited token (`read post spend` with `spend_limit_cents`) and drop the login token.

## Backend follow-ups found while building milestone 1

These need changes in GFCCoin. Milestone 1 doesn't change the backend.

- **No "list my wallets" endpoint.** The client remembers wallets per device. Suggested: `GET /wallets/mine`.
- **Transfer amounts truncate.** `int(amount * 100)` turns 0.29 GFC into 28 cents. The client adds a quarter
  cent to work around it. Fix: `round()`, or accept `amount_cents`.
- **Local test nodes can't turn off P2P.** The tracker relay and LAN broadcast always start, so a test node
  would copy test data onto the network. `Scripts/local_node.py` patches them out. Suggested: `GFC_P2P=off`.
- **Upload limits** to match the mobile budgets above.
