# GFC Client UI spec

The app is **phone-first: portrait, touch-first and one-handed**. Every screen
is built in C++ (`Source/GFCClient/UI`), so the project runs without
hand-authored assets, and each screen is designed to be replaced by a **Widget
Blueprint** later without C++ changes.

## Target device (milestone 1)

**Single target device: moto g power (2025), 1080×2388 px portrait at 390 dpi** (2.4375× scale, about
443 × 979 dp), Android 16, 60 Hz. Milestone 1 is laid out and checked for that one screen only. Tablets,
landscape, odd aspect ratios and other densities come later (see the roadmap).

- The UI scale curve maps 1080 px wide to 2.4375, so layouts are designed at **443 dp** wide.
- PC checks use the same layout: `Scripts\Run.ps1` opens 486×1075 (45% size, so it fits the dev monitor; same aspect and dp layout), and
  `-Full` opens 1080×2388. The editor's new-window play size is 1080×2388
  (`Config/DefaultEditorPerProjectUserSettings.ini`).
- Basic safe areas are kept (status bar and gesture bar), so nothing is clipped on this phone.
- Frame targets: 60 fps UI, 30 fps or better in the 3D viewer.

## Mobile rules (apply to Blueprint screens too)

- **Portrait**, one column. Android is locked to portrait (`Orientation=Portrait`).
- **Safe areas:** the root wraps everything in a `SafeZone` (notches, status bar, gesture bar). Keep it.
- **Tap targets at least 48 dp** tall; primary actions span the full width.
- **One-handed:** navigation sits in a bottom bar (Feed, Wallet). Primary actions sit under the content they act on.
- **No hover-dependent UI.** Buttons use `Touch Method: Precise Tap` so a drag that starts on a button scrolls its list.
- **Sizes are in dp.** The DPI curve (`DefaultEngine.ini`, `UIScaleCurve`) maps the target phone's 1080 px to 2.4375 (443 dp).
  Text: titles 22 to 26, body 13 to 16, captions 11 to 12. Inputs use 16 so phones don't zoom.
- **Bandwidth:** thumbnails first; full media only on tap (and after asking, on mobile data above 10 MB).
- **Preview while designing:** `.\Scripts\Run.ps1` (486x1075, the target phone at 45% size, mobile renderer), with `-Screen wallet|feed|seed:<id>|create`.

## Feed principles: the user is in control

People hate apps that commandeer the whole feed. GFC doesn't:

- **No autoplay takeover.** Nothing plays until it's tapped. Media never auto-plays full screen or with sound.
- **Video plays inline, muted, inside its card.** Tapping a video plays it within the card it's in, muted, with a
  "Sound on" button. Audio plays when its card is tapped, because the user asked for it.
- **No full-screen swipe loops.** The feed is an ordinary scrolling list. There's always a clear way back
  (Back, and the bottom bar), and no forced full-screen mode or trap.
- **No one dominates the feed.** No more than two posts in a row from the same creator (`FGFCFeedMixer`,
  tested in `GFC.Posting.FeedMixesCreators`). Otherwise the node's newest-first order is kept, and nothing is hidden.
- **Scrolling away stops things.** A card that leaves the screen stops playing and lets go of its media
  (see [cache-policy.md](cache-policy.md)).
- Avoid infinite-scroll dark patterns: show the count, keep a visible end of the list, and don't auto-load
  endlessly to keep people scrolling.

## How to replace a screen with a Blueprint

1. In the editor: **Add > User Interface > Widget Blueprint**, and pick the screen's C++ class as the parent
   (for example `GFCWalletScreen`).
2. Add widgets whose **names and types match the screen's bound parts** below. Required parts must exist (the
   Blueprint won't compile without them); optional parts may be left out and the screen skips them.
3. When a Blueprint provides a widget tree, the C++ default layout isn't built. All behaviour (API calls,
   validation, caching, navigation) stays in C++.
4. Point the app at your Blueprint:
   - Screens: on a Blueprint subclass of `GFCRootWidget`, set `LoginScreenClass`, `FeedScreenClass`,
     `WalletScreenClass`, `SeedDetailScreenClass`. Feed rows: `SeedCardClass` on the feed screen.
   - The root: on a Blueprint subclass of `GFCPlayerController`, set `RootWidgetClass`, and set that controller
     in a Blueprint `GFCGameMode` (Project Settings > Maps & Modes).
5. Optional: implement **On Screen Shown** to animate the screen in.

Blueprint hooks: `GFC Api Subsystem` (Get Client, Known Wallets, Needs Backup, Set Node), and every API call on
`GFC Api Client` (Login, Register, Get Feed, Send GFC, Export Wallet Backup and so on), each with a completion event.

## Theme: pure black (the only theme)

Milestone 1 has one theme, a pure black interface. It's easy on the eyes, and on
the **OLED screens most phones have, black pixels are switched off, which saves
battery**. That fits the mobile discipline. There's no light theme.

All colours are named tokens in **one place**: `FGFCTheme` in
`Source/GFCClient/UI/GFCUIStyle.h/.cpp`. Tune them there (sRGB hex):

| Token | Hex | Use |
|---|---|---|
| `Background` | `#000000` | App background, top and bottom bars, feed posts |
| `Surface` | `#16181C` | Raised surfaces and cards (wallet sections, login, media frames) |
| `SurfacePressed` | `#1D1F23` | Hover and pressed states |
| `Border` | `#2F3336` | Dividers, card and input outlines, secondary button outlines |
| `Text` | `#E7E9EA` | Primary text |
| `TextSecondary` | `#71767B` | Secondary text: handles, times, stats, hints |
| `Accent` | `#3B4CCA` | The GFC accent (the share pages' colour): primary buttons, focused inputs |
| `AccentText` | derived | `Accent` blended 45% toward white, for accent-coloured text (the brand, balance, creator links). `#3B4CCA` alone is too dark to read as text on black. |
| `Success` | `#00BA7C` | Success, rewards (incoming GFC), the verified badge |
| `Danger` | `#F4212E` | Errors |
| `Warning` | `#FFAD1F` | The wallet backup prompt (an outlined card and its button) |

Shapes: cards have 16 dp corners. Buttons are 24 dp pills: primary filled with `Accent`, secondary outlined
with `Border`. Inputs are black with a `Border` outline that turns `Accent` on focus. Font: engine Roboto.

The accent is deliberately GFC's own, not X's blue. The feed follows familiar social-feed conventions
(avatar, name, @handle, time, content, a stats row), but doesn't use X's logo, name, icons or distinctive marks.

**Icons:** milestone 1 uses text labels and letter avatars, so no icon assets are shipped yet. When icons are
added, use an open-licensed set, imported as a font or SVG-derived textures, and record the license here:
[Material Symbols](https://fonts.google.com/icons) (Apache License 2.0) or [Lucide](https://lucide.dev)
(ISC License). Don't use X's or other platforms' icon sets.

## Root (`GFCRootWidget`)

Top bar (brand, username, Log out / Log in), one status line, the active screen, and a bottom navigation bar.

| Part | Type | Required | Notes |
|---|---|---|---|
| `ScreenSwitcher` | WidgetSwitcher | yes | Screens are added at runtime. |
| `StatusText` | TextBlock | yes | Progress and errors (errors turn red). |
| `NavBar` | any widget | no | Bottom bar; hidden when logged out. |
| `FeedNavButton`, `WalletNavButton` | Button | no | In the bottom bar. |
| `LogoutButton` | Button | no | Revokes the token on the node, wipes the cache, shows login. |
| `LoginNavButton` | Button | no | Shown when logged out and viewing a shared seed. |
| `UserText` | TextBlock | no | The username. |

Behaviour:
- On launch, a deep link opens its seed. Otherwise a remembered session opens the feed, and no session shows login.
- A 401 from the node clears the session and returns to login.
- Leaving a screen calls its `OnScreenHidden`, which releases heavy content.
- Deep links arriving while running open the seed (`FGFCDeepLink::OnSeedLink`). Seeds open without logging in;
  "Log in" from a seed returns to it afterwards.

## Login (`GFCLoginScreen`)

One scrolling column with stacked full-width buttons.

| Part | Type | Required | Notes |
|---|---|---|---|
| `UsernameBox` | EditableTextBox | yes | |
| `PasswordBox` | EditableTextBox | yes | Set **Is Password**. Enter submits login. |
| `LoginButton` | Button | yes | Primary; full width. |
| `RegisterButton` | Button | yes | New accounts go to the wallet screen. |
| `EmailBox` | EditableTextBox | no | Optional e-mail for registering. |
| `NodeUrlBox`, `UseNodeButton`, `NodeText` | EditableTextBox, Button, TextBlock | no | Switch nodes (logs out of the old one). |

Must show: a warning that the password also unlocks wallet keys and can't be reset.

## Feed (`GFCFeedScreen`)

| Part | Type | Required | Notes |
|---|---|---|---|
| `SeedList` | any panel (ScrollBox) | yes | One `SeedCardClass` per seed, newest first. |
| `RefreshButton` | Button | no | |
| `EmptyText` | TextBlock | no | "Loading…", "N seeds", or the empty state. |

Lifecycle: every frame the feed checks which cards are on screen, with a quarter-screen margin. A card off screen
for longer than `OffscreenGraceSeconds` drops its thumbnail and releases its cache owner `card:<id>`. Coming back
re-requests it. Visible cards' thumbnails are kept when the app is backgrounded.

### Seed card (`GFCSeedCard`)

A social-feed post on black, separated by `Border` dividers: a letter avatar, then **name** @handle · time,
the title, the media (220 dp, cropped to fill, in a rounded frame), and a stats row (kind in `AccentText`, then
views, likes and comments).

| Part | Type | Required | Notes |
|---|---|---|---|
| `CardButton` | Button | yes | Opens the seed detail. Precise Tap, so swipes scroll. |
| `ThumbImage` | Image | yes | From the cache; hidden until it arrives. |
| `TitleText`, `OwnerText` | TextBlock | yes | "by {owner}". The title hides on a text post titled from its own first line. |
| `BodyText` | TextBlock | no | The description: a text post's text (up to 600 characters in the feed) or a caption (200). |
| `MediaFrame` | any widget | no | The media frame; collapsed for text posts. |
| `KindText`, `MetaText` | TextBlock | no | "3D model"; "N views  N likes  N comments". |
| `HandleText` | TextBlock | no | "@owner · 3h". |
| `AvatarSlot` | SizeBox | no | Holds the avatar (a letter for now; profile images later). |
| `RepostText` | TextBlock | no | "Repost of @original. Credit goes to the original." when the node says so. |
| `MoreButton` | Button | no | "•••" in the header: Report post (with a reason) or Block @owner, in an action sheet (`GFCActionSheet`). The post disappears at once; blocked accounts' posts stay out of the feed. |
| `MediaButton`, `MediaLabel` | Button, TextBlock | no | Over the media: "▶ Play video" / "▶ Play audio" (plays inline), "View in 3D" (opens the seed). |
| `SoundButton`, `SoundLabel` | Button, TextBlock | no | Inline video only: "Sound on" / "Mute". |

## Seed detail (`GFCSeedDetailScreen`)

Thumbnail first. On tap, the full content plays in the app.

| Part | Type | Required | Notes |
|---|---|---|---|
| `TitleText` | TextBlock | yes | |
| `PrimaryButton` | Button | yes | "View in 3D", "View full image", "Play video", "Play audio". Then "Reset view" or "Play / pause". Asks first on mobile data. |
| `PrimaryLabel` | TextBlock | no | The button's label (updated per state). |
| `ShareButton` | Button | yes | Opens the X intent (`https://x.com/intent/post?text=…&url=…`); copies the link if no browser opens. |
| `CopyLinkButton` | Button | no | Copies `https://node.galacticfederation.tech/p/{id}` (opens the app via App Links). |
| `BackButton` | Button | no | Back to the feed (or login when logged out). |
| `PreviewImage` | Image | no | The thumbnail, then the full image or the video. |
| `ModelView` | GFCModelView | no | The 3D view; created in code if absent. Turns slowly on its own (a turntable) and plays the model's first animation on a loop; a touch stops the turn, which resumes 4 s after letting go. Drag: orbit. Pinch or Ctrl + wheel: zoom (the plain wheel scrolls the page). Two fingers or right drag: pan. Double click: reset. |
| `PreviewNote` | TextBlock | no | Over the preview: progress, gesture hints, errors such as "over the triangle limit". Its parent hides when empty. |
| `OpenMediaButton` | Button | no | Only for kinds the app can't show ("Open the original file"). |
| `MoreButton` | Button | no | "•••" next to Back: report or block, as on the card. Leaves the post once it's hidden. |
| `OwnerText`, `DescriptionText`, `MetaText`, `ShareLinkText` | TextBlock | no | A text post's text is `DescriptionText`, at 18 pt. |
| `PreviewFrame` | any widget | no | The preview area; collapsed with `PrimaryButton` for text posts. |

States: Thumbnail → (Confirm on mobile data) → Downloading → Preparing → Ready, or Failed with a reason
(too big, low storage, timeout, unsupported model feature).

Lifecycle: the seed is the one live heavy item. Leaving the screen, opening another seed, backgrounding or
logging out releases the actor, mesh, textures, render target, media player and temp file. Qualified views
are reported once (see docs/roadmap.md).

## Create (`GFCCreateScreen`)

Opened from **+ Post** in the bottom bar. Top to bottom, in the order you do it: write, add something
(optional), title, post. A post can be just text: it's uploaded as a small `.txt` seed (`<cache>/outbox/`,
deleted after posting) with the text as its description, titled from its first line when the title is empty.

| Part | Type | Required | Notes |
|---|---|---|---|
| `PhotoButton` | Button | yes | System camera, photo. Phone only (disabled on PC). |
| `VideoButton` | Button | yes | System camera, video. Phone only. |
| `BodyBox` | MultiLineEditableTextBox | no | "What's on your mind?" The whole post, or the caption. Up to 5000 characters. |
| `GalleryButton` | Button | yes | System photo picker (photos and videos); a file dialog on PC. |
| `FileButton` | Button | no | "3D model or other file": the system file browser (`ACTION_OPEN_DOCUMENT`, any type), for `.glb`/`.gltf` and audio. |
| `TitleBox` | EditableTextBox | yes | Prefilled with the file name when there's no text. Optional for text posts. |
| `PostButton` | Button | yes | Uploads to `/seeds/upload`. Enabled once there's text or media. |
| `MediaPanel`, `RemoveMediaButton` | any widget, Button | no | The chosen media's preview and details; hidden when there's none. Remove drops it. A 3D model previews live (turntable, animation, touch), checked against the phone budgets; the view when Post is tapped becomes the thumbnail. |
| `HeaderPostButton` | Button | no | Post, in the "New post" header, so it's in reach however long the form is. Choosing media scrolls the form down to the title and Post. |
| `PreviewImage`, `FileText` | Image, TextBlock | no | Preview (image, or the video's first frame); name, size, "Saved to your gallery", or the 200 MB limit. |
| `UploadProgress`, `ProgressText`, `CancelButton` | ProgressBar, TextBlock, Button | no | "Uploading 12.3 MB of 48.0 MB". Cancel stops it. |
| `ResultText`, `ViewPostButton` | TextBlock, Button | no | "Posted!", or "This looks like a repost of @original. They'll get the credit." |

Rules:
- Captures are saved to the phone's gallery (Pictures/GFC, Movies/GFC) and stay the user's. The app's temporary
  copy is deleted after posting or cancelling.
- Ask for permissions only when they're needed: WRITE_EXTERNAL_STORAGE on Android 9 and older, when Photo or Video
  is tapped. The camera and picker need no other permissions.
- Over 200 MB (the gateway's request limit) is refused before uploading. Network errors and 5xx answers are retried
  twice with backoff.

## Earnings burst (`GFCRewardBurst`)

When the user earns GFC: the ka-ching (`Content/GFC/Sounds/ka_ching.wav`, from the Altered Earth sound effects,
staged as a plain file and played through a procedural sound wave) and about 20 gold coins that burst up from the
bottom bar, flip, fade near the top and leave sparkles, under a big gold "+1,000 GFC" that pops in. Three seconds,
above everything, never takes a tap.

It plays:
- after a post, when `/seeds/upload` answers `first_post_bonus` ("First post bonus!"). `first_post_bonus_pending`
  instead says "Create a wallet to collect your first-post bonus."
- at login, and on launch with a remembered session, when the user's wallets went up since they last saw them
  ("You earned GFC while you were away!"). Each known wallet keeps `LastSeenCents` in the session save; a wallet's
  first sighting isn't a gain, and a balance shown on the Wallet screen counts as seen.

Preview it in a Development build with the console command `GFC.Celebrate [amount]`. On the phone, run
`adb shell am broadcast -a android.intent.action.RUN -e cmd "GFC.Celebrate 1000"`.
## Wallet (`GFCWalletScreen`)

One scrolling column: balance, backup prompt, send, wallets, history. The node has no "list my wallets" endpoint
yet, so the picker shows wallets this device created or was given ("Add by ID"), per user and node.

| Part | Type | Required | Notes |
|---|---|---|---|
| `WalletPicker` | ComboBoxString | yes | "name (abcd1234…ef01)". |
| `BalanceText` | TextBlock | yes | "12.34 GFC", from `balance_cents`. |
| `WalletIdText`, `CopyIdButton`, `RefreshButton`, `NoWalletText` | | no | Copy ID copies the full ID. |
| `CreateWalletButton` | Button | yes | |
| `VerifyPanel`, `VerifyPhoneButton` | any widget, Button | no | "Verify your phone" card at the top, shown while the account isn't verified (`GET /verify/status`). Opens the node's verification page in the browser (`POST /verify/link`); back in the app (via `gfc://verified` or just returning), the status is checked and the earnings burst plays for the welcome grant and any first post bonus. |
| `NewWalletNameBox`, `AddWalletIdBox`, `AddWalletButton` | | no | |
| `BackupPanel` | any widget | yes | **The backup prompt. Keep it prominent.** Visible whenever the user has a wallet. |
| `BackupPasswordBox`, `BackupPassphraseBox` | EditableTextBox | yes | Both password fields. Passphrase 12+ characters. |
| `BackupConfirmBox` | EditableTextBox | no | Must match the passphrase. |
| `ExportButton` | Button | yes | Calls `/wallets/export` and saves the encrypted file. |
| `BackupTitleText`, `BackupResultText` | TextBlock | no | The title switches to "Backed up" afterwards; the result shows where the file went. |
| `RecipientBox`, `AmountBox` | EditableTextBox | yes | Amount like `2.50`, at most two decimals. |
| `ReasonBox` | EditableTextBox | no | Note. |
| `SendButton` | Button | yes | |
| `HistoryList` | any panel | yes | Rows: "+/−amount GFC", note, counterparty, type, time. |

Backup copy (keep the meaning): wallet keys live encrypted on the node and are unlocked by the password;
forgetting it or losing the node's data loses the GFC; there is no reset; keep the file and passphrase safe and
apart; anyone with both can spend.
