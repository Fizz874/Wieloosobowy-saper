**[🇬🇧 EN Version Below](#english-version)**

🇵🇱 PL
# 💣 Wieloosobowy Saper

## 1. Opis projektu

Projekt zespołowy, którego celem było stworzenie sieciowej gry w stylu klasycznego [Sapera](https://pl.wikipedia.org/wiki/Saper_(gra_komputerowa)), z możliwością udziału wielu graczy w czasie rzeczywistym. Gra oparta jest na architekturze klient-serwer. Serwer zarządza przebiegiem rozgrywki, a gracze łączą się do niego za pomocą aplikacji klienckiej.

### 🔹 Przebieg gry

- Po połączeniu z serwerem gracz podaje **nick**. Jeśli jest zajęty lub niepoprawny, serwer prosi o inny.
- Jeśli gra się jeszcze nie rozpoczęła, gracz trafia do **lobby** z planszą i odliczaniem.
- Jeśli gra już trwa – gracz **dołącza do rozgrywki**.

### 🔹 Zasady

- Gracze widzą wspólną planszę z nieodkrytymi polami.
- Każdy gracz przełącza się między trybem:
  - **odkrywania pól**
  - **oznaczania bomb**
- Odsłonięcie lub oznaczenie pola jest widoczne dla wszystkich.
- Punkty otrzymuje tylko gracz, który **jako pierwszy kliknął dane pole**.

- Gracze widzą:
  - **ranking punktowy**
  - **zegar gry**
- Gra kończy się, gdy:
  - wszystkie pola zostaną oznaczone/odsłonięte
  - lub czas gry się skończy

Po zakończeniu gry wyświetlany jest **ranking końcowy** – również dla graczy, którzy się rozłączyli. Każdy gracz może dołączyć do kolejnej rozgrywki.

---

## 2. Wykorzystane technologie

### 🖥️ Środowisko

- **Serwer**: Linux (C++)
- **Klient**: Windows (C#, aplikacja okienkowa)

### 🛠️ Technologie

- **C++** – logika serwera
- **C# / .NET (Windows Forms)** – interfejs kliencki
- **Sockety TCP/IP** – komunikacja klient-serwer
- **Plik konfiguracyjny `config.txt`** – definiuje parametry rozgrywki

---

## 3. Konfiguracja serwera

Plik `config.txt` umożliwia dostosowanie zasad gry bez rekompilacji:

- `port` – numer portu serwera (zakres: 1024–65535)
- `bomb_density` – prawdopodobieństwo wystąpienia bomby (im większa liczba, tym mniej bomb; zakres: 2–10)
- `board_edge` – rozmiar jednej krawędzi planszy (zakres: 20–512)
- Punktacja:
  - `blank_hit` – za odkrycie pustego pola bez bomby
  - `blank_flag` – za błędne oznaczenie pustego pola
  - `bomb_hit` – za kliknięcie bomby
  - `bomb_flag` – za poprawne oznaczenie bomby
  - `blank_empty` – za odkrycie pustego pola, które odkrywa sąsiednie pola
- Czas:
  - `waiting_timeout` – czas oczekiwania na graczy przed rozpoczęciem (sekundy)
  - `game_timeout` – maksymalny czas gry (sekundy)
  - `restart_timeout` – czas oczekiwania przed kolejną rozgrywką (sekundy)

> Wartości spoza dozwolonego zakresu zostaną zastąpione domyślnymi.

## 4. Budowa i uruchomienie

### 🔧 Kompilacja serwera (Linux)

**Wymagania**: `g++`, `cmake`

**Instrukcja:**

1. Otwórz terminal w katalogu z projektem.
2. Wykonaj następujące polecenia:

```
mkdir build
cd build
cmake ..
make
./Server
```

Serwer automatycznie wczyta konfigurację z pliku `config.txt`.

---

### 🖼️ Uruchomienie klienta (Windows)

**Instrukcja:**

1. Otwórz plik `MultiSaper.sln` w **Visual Studio**.
2. Skonfiguruj tryb uruchamiania: `Debug` lub `Release`.
3. Kliknij `Start` lub naciśnij `F5`, aby uruchomić klienta.

---

## 5. Przyszłe rozszerzenia

- Obsługa czatu między graczami
- Tryb "widza"
- Wersja mobilna klienta

---

## 6. Autorzy

- ✍️ [Artur Strzelecki](https://github.com/0Artur1)
- ✍️ [Filip Baranowski](https://github.com/Fizz874)

---

<a id="english-version"></a>
# 💣 Multiplayer Minesweeper

## 1. Project Description

A team project aimed at creating a real-time **multiplayer** [Minesweeper](https://en.wikipedia.org/wiki/Minesweeper_(video_game)) game based on a client-server architecture. The server manages the game logic, while players connect via a dedicated Windows client.

### 🔹 Game Flow

- When connecting to the server, a player provides a **nickname**. If it’s taken or invalid, the server asks for a different one.
- If the game hasn't started yet, the player enters the **lobby** with a countdown and game board.
- If a game is already in progress, the player **joins the ongoing session**.

### 🔹 Game Rules

- All players see the same board with unrevealed tiles.
- Players can toggle between two modes:
  - **Reveal mode**
  - **Flag mode**
- Revealing or flagging a tile is **synchronized across all clients**.
- Points are awarded **only to the first player** who interacts with a tile.

- Players can see:
  - **live ranking**
  - **game countdown timer**
- The game ends when:
  - all tiles are flagged or revealed
  - or the timer reaches zero

After the game ends, the **final scoreboard** is displayed — including disconnected players. Players can then join the next round.

---

## 2. Technologies Used

### 🖥️ Environment

- **Server**: Linux (C++)
- **Client**: Windows (C#, GUI)

### 🛠️ Technologies

- **C++** – core server logic
- **C# / .NET (Windows Forms)** – graphical Windows client
- **TCP/IP Sockets** – for real-time networking
- **Configuration File (`config.txt`)** – defines game parameters

---

## 3. Server Configuration

The `config.txt` file allows adjusting gameplay parameters without recompiling.

- `port` – server port (range: 1024–65535)
- `bomb_density` – bomb appearance probability (higher value = fewer bombs; range: 2–10)
- `board_edge` – board size (number of tiles per edge; range: 20–512)
- Scoring:
  - `blank_hit` – reveal tile without bomb
  - `blank_flag` – incorrectly flag empty tile
  - `bomb_hit` – click bomb
  - `bomb_flag` – correctly flag bomb
  - `blank_empty` – reveal empty tile with auto-reveal area
- Timing:
  - `waiting_timeout` – wait time before game starts (seconds)
  - `game_timeout` – maximum game duration (seconds)
  - `restart_timeout` – wait time before restarting (seconds)

> Out-of-range values will be replaced with default ones.


## 4. Building & Running the Game

### 🔧 Building the Server (Linux)

**Requirements**: `g++`, `cmake`

**Instructions:**

1. Open terminal in the project directory.
2. Run the following commands:

```
mkdir build
cd build
cmake ..
make
./Server
```

The server will automatically load settings from `config.txt`.

---

### 🖼️ Running the Client (Windows)

**Instructions:**

1. Open `MultiSaper.sln` using **Visual Studio**.
2. Select `Debug` or `Release` build.
3. Click `Start` or press `F5` to launch the client.

---

## 5. Future Improvements

- In-game chat
- Spectator mode
- Mobile client version

---

## 6. Authors

- ✍️ [Artur Strzelecki](https://github.com/0Artur1)
- ✍️ [Filip Baranowski](https://github.com/Fizz874) 

---
