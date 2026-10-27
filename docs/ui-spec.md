# GFC Client UI spec

Milestone 1 builds every screen in C++ (`Source/GFCClient/UI`), so the project
runs with no editor-made assets. Each screen is designed to be replaced by a
**Widget Blueprint** later, without C++ changes.

## How to replace a screen with a Blueprint

1. In the editor: **Add > User Interface > Widget Blueprint**, and pick the
   screen's C++ class as the parent (for example `GFCWalletScreen`).
2. Add widgets whose **names and types match the screen's bound parts** below.
   Required parts must exist (the Blueprint won't compile without them);
   optional parts may be left out and the screen skips them.
3. When a Blueprint provides a widget tree, the C++ default layout isn't built.
   All behaviour (API calls, validation, navigation) stays in C++.
4. Point the app at your Blueprint:
   - Screens: on a Blueprint subclass of `GFCRootWidget`, set
     `LoginScreenClass`, `FeedScreenClass`, `WalletScreenClass`,
     `SeedDetailScreenClass`. Feed rows: `SeedCardClass` on the feed screen.
   - The root: on a Blueprint subclass of `GFCPlayerController`, set
     `RootWidgetClass`, and set that controller in a Blueprint `GFCGameMode`
     (Project Settings > Maps & Modes).
5. Optional: implement **On Screen Shown** to animate the screen in.

Other Blueprint hooks: `GFC Api Subsystem` (Get Client, Known Wallets, Needs
Backup, Set Node) and every API call on `GFC Api Client` (Login, Register, Get
Feed, Send GFC, Export Wallet Backup, ...) with a completion event.

Use `-GFCScreen=wallet`, `-GFCScreen=feed` or `-GFCScreen=seed:<id>` on the
command line to open a screen directly while designing.

## Visual language (default layout)

| Token | Use | Linear colour |
|---|---|---|
| Background | App background | (0.010, 0.014, 0.030) |
| Panel | Cards | (0.030, 0.040, 0.075) |
| PanelRaised | Secondary buttons, rows | (0.050, 0.064, 0.115) |
| Accent | Primary buttons, balance, creator names | (0.020, 0.560, 0.910) |
| Warning | Backup prompt | (0.910, 0.380, 0.020) |
| Danger | Errors in the status line | (0.800, 0.090, 0.070) |
| Success | Incoming amounts | (0.110, 0.650, 0.250) |

Font: engine Roboto. Titles 22 to 26 bold, body 12 to 14, captions 10 to 11.

## Root (`GFCRootWidget`)

The app shell: top bar, one status line, and a switcher holding the screens.

| Part | Type | Required | Notes |
|---|---|---|---|
| `ScreenSwitcher` | WidgetSwitcher | yes | Screens are added to it at runtime. |
| `StatusText` | TextBlock | yes | Progress and errors (errors turn red). |
| `NavBar` | any widget | no | Hidden when logged out. |
| `FeedNavButton`, `WalletNavButton` | Button | no | Navigation. |
| `LogoutButton` | Button | no | Revokes the token on the node, then shows login. |
| `UserText` | TextBlock | no | "Signed in as …". |

Behaviour: on launch, a remembered session opens the feed, otherwise login.
Any 401 from the node clears the session and returns to login.

## Login (`GFCLoginScreen`)

| Part | Type | Required | Notes |
|---|---|---|---|
| `UsernameBox` | EditableTextBox | yes | |
| `PasswordBox` | EditableTextBox | yes | Set **Is Password**. Enter submits login. |
| `LoginButton` | Button | yes | |
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

### Seed card (`GFCSeedCard`)

| Part | Type | Required | Notes |
|---|---|---|---|
| `CardButton` | Button | yes | Opens the seed detail. |
| `ThumbImage` | Image | yes | Loaded from the node; hidden until it arrives. |
| `TitleText`, `OwnerText` | TextBlock | yes | "by {owner}". |
| `KindText`, `MetaText` | TextBlock | no | "3D model"; "N views · N likes · N comments". |

## Seed detail (`GFCSeedDetailScreen`)

| Part | Type | Required | Notes |
|---|---|---|---|
| `TitleText` | TextBlock | yes | |
| `ShareButton` | Button | yes | Opens the X intent (`https://x.com/intent/post?text=…&url=…`) in the browser; copies the link if no browser opens. |
| `CopyLinkButton` | Button | no | Copies `https://galacticfederation.tech/p/{id}`. |
| `BackButton` | Button | no | Back to the feed. |
| `OpenMediaButton` | Button | no | Opens the original file from the node. |
| `PreviewImage` | Image | no | The thumbnail. Milestone 2 puts the 3D viewer here. |
| `PreviewNote` | TextBlock | no | Shown over the preview: "3D model · interactive 3D view coming in milestone 2". Its parent is hidden when there's no note. |
| `OwnerText`, `DescriptionText`, `MetaText`, `ShareLinkText` | TextBlock | no | |

## Wallet (`GFCWalletScreen`)

The node has no "list my wallets" endpoint yet, so the picker shows wallets this
device created or was given ("Add by ID"), per user and node.

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
| `BackupTitleText`, `BackupResultText` | TextBlock | no | Title switches to "Backed up" afterwards; result shows the saved path. |
| `RecipientBox`, `AmountBox` | EditableTextBox | yes | Amount like `2.50`, max two decimals. |
| `ReasonBox` | EditableTextBox | no | Note. |
| `SendButton` | Button | yes | |
| `HistoryList` | any panel (ScrollBox) | yes | Rows: "+/−amount GFC", note, counterparty, type, time. |

Backup copy (keep the meaning): wallet keys live encrypted on the node and are
unlocked by the password; forgetting it or losing the node's data loses the GFC;
there is no reset; keep the file and passphrase safe and apart; anyone with both
can spend.

## Mobile notes

The default layout is desktop-first (two columns on the wallet screen). For
Android, make Blueprint screens with a single column, 48 dp touch targets, and
the backup prompt as a full-screen step after the first wallet is created.
