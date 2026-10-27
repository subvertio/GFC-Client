# GFC Client UI spec

The app is **phone-first: portrait, touch-first and one-handed**. Every screen
is built in C++ (`Source/GFCClient/UI`), so the project runs without
hand-authored assets, and each screen is designed to be replaced by a **Widget
Blueprint** later without C++ changes.

## Mobile rules (apply to Blueprint screens too)

- **Portrait**, one column. Android is locked to portrait (`Orientation=Portrait`).
- **Safe areas:** the root wraps everything in a `SafeZone` (notches, status bar, gesture bar). Keep it.
- **Tap targets at least 48 dp** tall; primary actions span the full width.
- **One-handed:** navigation sits in a bottom bar (Feed, Wallet). Primary actions sit under the content they act on.
- **No hover-dependent UI.** Buttons use `Touch Method: Precise Tap` so a drag that starts on a button scrolls its list.
- **Sizes are in dp.** The DPI curve (`DefaultEngine.ini`, `UIScaleCurve`) maps a 412 dp-wide phone to scale 1.0.
  Text: titles 22 to 26, body 13 to 16, captions 11 to 12. Inputs use 16 so phones don't zoom.
- **Bandwidth:** thumbnails first; full media only on tap (and after asking, on mobile data above 10 MB).
- **Preview while designing:** `.\Scripts\Run.ps1` (432x900, mobile renderer), with `-Screen wallet|feed|seed:<id>`.

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

## Visual language (default layout)

| Token | Use | Linear colour |
|---|---|---|
| Background | App background | (0.010, 0.014, 0.030) |
| Panel | Cards, top and bottom bars | (0.030, 0.040, 0.075) |
| PanelRaised | Secondary buttons, rows, feed cards | (0.050, 0.064, 0.115) |
| Accent | Primary buttons, balance, creator names | (0.020, 0.560, 0.910) |
| Warning | Backup prompt | (0.910, 0.380, 0.020) |
| Danger | Errors in the status line | (0.800, 0.090, 0.070) |
| Success | Incoming amounts | (0.110, 0.650, 0.250) |

Font: engine Roboto.

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

Full-width card: thumbnail on top (200 dp, cropped to fill), then title, creator, kind and stats.

| Part | Type | Required | Notes |
|---|---|---|---|
| `CardButton` | Button | yes | Opens the seed detail. Precise Tap, so swipes scroll. |
| `ThumbImage` | Image | yes | From the cache; hidden until it arrives. |
| `TitleText`, `OwnerText` | TextBlock | yes | "by {owner}". |
| `KindText`, `MetaText` | TextBlock | no | "3D model"; "N views · N likes · N comments". |

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
| `ModelView` | GFCModelView | no | The 3D view; created in code if absent. Drag: orbit. Pinch or wheel: zoom. Two fingers or right drag: pan. Double click: reset. |
| `PreviewNote` | TextBlock | no | Over the preview: progress, gesture hints, errors such as "over the triangle limit". Its parent hides when empty. |
| `OpenMediaButton` | Button | no | Only for kinds the app can't show ("Open the original file"). |
| `OwnerText`, `DescriptionText`, `MetaText`, `ShareLinkText` | TextBlock | no | |

States: Thumbnail → (Confirm on mobile data) → Downloading → Preparing → Ready, or Failed with a reason
(too big, low storage, timeout, unsupported model feature).

Lifecycle: the seed is the one live heavy item. Leaving the screen, opening another seed, backgrounding or
logging out releases the actor, mesh, textures, render target, media player and temp file. Qualified views
are reported once (see docs/roadmap.md).

## Wallet (`GFCWalletScreen`)

One scrolling column: balance, backup prompt, send, wallets, history. The node has no "list my wallets" endpoint
yet, so the picker shows wallets this device created or was given ("Add by ID"), per user and node.

| Part | Type | Required | Notes |
|---|---|---|---|
| `WalletPicker` | ComboBoxString | yes | "name (abcd1234…ef01)". |
| `BalanceText` | TextBlock | yes | "12.34 GFC", from `balance_cents`. |
| `WalletIdText`, `CopyIdButton`, `RefreshButton`, `NoWalletText` | | no | Copy ID copies the full ID. |
| `CreateWalletButton` | Button | yes | |
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
