# GFC Client roadmap

## Milestone 1: PC client (this PR)

- C++ project, `GFCApi` runtime module (HTTP + Json), Blueprint-callable.
- Screens built in C++ UMG: login/register, wallet (balance, send, history,
  backup export), feed, seed detail, share to X / copy link.
- Session token protected with DPAPI on Windows.
- Automation tests (`GFC.Api.*`) and an opt-in end-to-end test against a local node (`GFC.E2E.LocalNode`).
- 3D seeds show their thumbnail plus "interactive 3D view coming in milestone 2".

## Milestone 2: Echo Hunt and a 3D viewer

### Echo Hunt (see `docs/echo-hunt.md` in GFCCoin)

- 3D audio: spatialised emitters (MetaSounds + a spatialisation plugin; on Android,
  check the platform's HRTF support and fall back to panning).
- Gyro: `UInputSettings`/Enhanced Input motion on Android (`GetInputMotionState`
  or the `MotionController`/tilt events) to aim the listener. PC fallback: mouse.
- Rounds from `POST /echohunt/start`, answers to `POST /echohunt/submit`.
- **Play Integrity**: request an integrity token in Java and pass it to C++.
  - Add a UPL file (`Source/GFCClient/GFCClient_UPL.xml`) that adds the
    `com.google.android.play:integrity` Gradle dependency and a small Java
    helper (`StandardIntegrityManager`, prepare once, then `request()` with the
    node's nonce/request hash).
  - Call it through JNI (`FJavaWrapper::CallVoidMethod` on `GameActivity`) and
    get the token back through a `JNI_METHOD` callback.
  - Send it with the Echo Hunt submission. The node verifies it (needs
    `GFC_PLAY_INTEGRITY_SA_FILE` and `GFC_ANDROID_PACKAGE` configured on the node; not set up yet).
  - Never fake attestation on PC: PC plays rely on phone verification only.

### Runtime 3D viewer (glTF / .glb)

Seeds are user uploads, so they must load at runtime from a URL, on PC and Android.

| Option | Runtime in packaged builds | Android | Notes |
|---|---|---|---|
| **glTFRuntime** (rdeioris, MIT, GitHub + Fab) | Yes, designed for it | Yes (reported by users; verify on our devices) | Static/skeletal meshes, materials, textures, animations, `.glb`. Loads from memory, so we can feed it bytes from our own HTTP client. Mature, widely used (Ready Player Me used it). Known hitch on big files: load on a background step and show a spinner. |
| Epic **Interchange** runtime import (`UInterchangeManager::ImportAssetAsync`) | Partly | Unclear | Built in, but a 5.7.4 issue report (UE-377300) describes broken meshes, materials and textures from the runtime factories in Shipping builds, needing large workarounds and engine changes. |
| Epic glTF Importer / Datasmith glTF | Editor only | n/a | Deprecated in favour of Interchange; not for user content. |
| Write our own loader (cgltf + `UProceduralMeshComponent` / `UDynamicMesh`) | Yes | Yes | Full control and tiny, but materials, skinning and animation are a lot of work. |

**Recommendation:** use **glTFRuntime** for milestone 2. Before adopting it,
confirm that a release targets UE 5.7 (the plugin's GitHub/Fab page) and that a test
`.glb` loads on a physical Android device. Keep it behind a small
`IGFCModelLoader` interface so we can swap to Interchange if Epic fixes runtime
import. Guard rails: size cap (e.g. 50 MB) before download, triangle/texture
limits after parsing, load off the game thread where the plugin allows it,
and never execute anything from the file (glTF has no scripts, but extensions may
point at external URIs: only allow the embedded buffers of `.glb`).

The detail screen already has a preview area (`PreviewImage` + `PreviewNote`)
where the viewer goes: replace the thumbnail with a `USceneCaptureComponent2D`
render target, or open a dedicated 3D level with orbit controls.

## Milestone 3: phone verification, uploads, tips

- **Phone verification** (`/verify/phone/start` and `/check`). Public nodes
  require a **Cloudflare Turnstile** token, which needs a browser: show the
  widget in a `UWebBrowser` (WebBrowserWidget plugin; on Android it uses the
  system WebView), on a small page that posts the token back through
  `window.ue` / a JS binding. Then send it as `turnstile_token`.
  Verification unlocks the 100 GFC grant and rewarded plays.
- **Rewarded plays**: call `POST /seeds/{id}/played` when media is played.
- **Uploads** (`/seeds/upload`, multipart): file picker on PC (`IDesktopPlatform`
  is editor-only, so use the Windows common dialog or a native plugin), Android
  Storage Access Framework via UPL/JNI. Generate a JPG thumbnail client-side.
- **Tips**: a "Tip creator" button on the seed detail that sends to
  `reward_wallet` with `POST /transactions/create`.
- Likes, comments, bookmarks, profiles, search.
- **Scoped tokens**: after login, mint a limited token (`POST /auth/tokens`,
  `read post spend` with a `spend_limit_cents`) and drop the login token.

## Android build (after milestone 1)

- Store the token in the **Android Keystore** (see `TODO(android-keystore)` in
  `GFCSessionStore.h`).
- Wallet backup export: hand the file to the share sheet / Storage Access
  Framework instead of `Saved/Backups` (`TODO(android)` in `GFCApiSubsystem.h`).
- Touch-sized UI: the code layout is desktop-first; designers should make
  Blueprint screens for phones (see `docs/ui-spec.md`).
- Deep links: handle `gfc://seed/{id}` and the verified `https://galacticfederation.tech/p/...`
  app links (the node serves `/.well-known/assetlinks.json`).

## Backend follow-ups found while building milestone 1

These need changes in GFCCoin. Milestone 1 doesn't change the backend.

- **No "list my wallets" endpoint.** The client remembers wallets it created or
  was told about, per device. Suggested: `GET /wallets/mine` (read scope) returning
  the caller's wallets with balances.
- **Transfer amounts truncate.** `/transactions/create` does `int(amount * 100)`,
  so 0.29 GFC becomes 28 cents. The client works around it by adding a
  quarter cent before sending. Suggested fix: `round(amount * 100)`, or accept
  `amount_cents`.
- **Local test nodes can't turn off P2P.** The tracker relay and LAN broadcast
  always start, so a local test node would copy test data onto the network.
  `Scripts/local_node.py` patches them out. Suggested: a `GFC_P2P=off` switch.
