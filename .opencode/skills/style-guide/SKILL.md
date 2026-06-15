---
name: style-guide
description: >-
  ESP32 Bridge Dashboard Style Guide — Use when styling the bridge web dashboard
  or inline ESP8266 client pages. Covers CSS variables, cards, badges, buttons,
  layout, and responsive design.
---

# ESP32 Bridge — Style Guide (Dashboard Web)

## Paleta de Cores (CSS Variables)

```css
:root {
  --bg: #f4f7fc;           /* fundo da página */
  --surface: #ffffff;       /* cartões */
  --surface-2: #f9fbff;     /* device list item bg */
  --text: #24324a;          /* texto principal */
  --muted: #7a8ba3;         /* texto secundário */
  --primary: #3498db;       /* azul destaque */
  --primary-strong: #2d7dff;
  --border: #e6edf7;
  --shadow: 0 12px 30px rgba(31, 58, 88, 0.08);
  --success: #2e7d32;       /* verde online/ligado */
  --danger: #c62828;        /* vermelho offline/desligado */
}
```

## Componentes

### Cartão (`.card`)
- bg `var(--surface)`, border `var(--border) 1px`, border-radius `18px`, shadow `var(--shadow)`
- padding `16px`
- Título `<h2>` cor `var(--primary)`, tamanho `0.95rem`

### Hero
- Flex row, gap `16px`
- `.hero-card`: flexível, padding `18px 20px`
- `.eyebrow`: uppercase tracking, `0.72rem`, cor `var(--primary)`
- `<h1>`: `1.5rem`

### Summary Grid (`.summary-grid`)
- 3 colunas iguais, gap `12px`
- `.metric`: surface + border + radius `18px`, padding `14px 16px`
- `.metric-label`: uppercase `0.72rem`, cor `var(--muted)`
- `.metric-value`: `1.02rem`, bold, cor `var(--text)`

### Content Grid (`.content`)
- 2 colunas: `minmax(280px, 340px)` (sidebar) + `minmax(0, 1fr)` (main)
- gap `16px`

### Row (`.row`)
- Flex space-between, padding `10px 0`, border-bottom `1px solid var(--border)`
- `.label`: cor `var(--muted)`
- `.value`: bold, cor `var(--text)`

### Badge (`.badge`)
- Inline-flex, min-width `72px`, padding `4px 10px`, border-radius `999px`
- `0.72rem`, bold, uppercase
- `.badge.on`: bg green `rgba(46, 125, 50, 0.12)`, cor `var(--success)`
- `.badge.off`: bg red `rgba(198, 40, 40, 0.12)`, cor `var(--danger)`

### Device List Item (`.device`)
- bg `var(--surface-2)`, border `var(--border) 1px`, border-radius `14px`
- padding `14px`
- `.device-head`: flex row, gap `8px`, wrap
- `.dev-name`: bold
- `.dev-meta`: `0.76rem`, cor `var(--muted)`, margin `6px 0 8px`
- `.dev-state`: `0.9rem`, cor `var(--muted)`

### LED Indicator (`.led`)
- `9px` círculo, box-shadow `0 0 0 4px rgba(52, 152, 219, 0.08)`
- `.led.on`: bg `var(--success)`
- `.led.off`: bg `#96a3b6`

### Botões
- `.btn`: surface + border `1px solid var(--primary)`, radius `10px`, padding `10px 16px`
- `0.85rem`, semibold, cor `var(--primary)`
- Hover: bg `var(--primary)`, cor `#fff`
- `.btn-danger`: border/cor `var(--danger)`, hover bg `var(--danger)`
- `.btn:disabled`: opacity `0.5`, cursor not-allowed

### Copy Button (`.cpy`)
- bg `var(--primary)`, cor `#fff`, radius `6px`, padding `3px 10px`
- `0.72rem`, bold, uppercase, cursor pointer

### Empty State (`.empty`)
- Text-align center, padding `36px 16px`, cor `var(--muted)`

### Code (`.code`)
- Monospace stack, `0.82rem`, bg `var(--surface-2)`, padding `3px 8px`, radius `6px`

## Background do Body
```css
body {
  background:
    radial-gradient(circle at top left, rgba(52, 152, 219, 0.10), transparent 26%),
    linear-gradient(180deg, #f8fbff 0%, #f4f7fc 100%);
}
```

## Responsivo (max-width: 900px)
- Hero e Content: 1 coluna
- Summary grid: 1 coluna

---

# ESP8266 Clients — Style Guide (Inline)

## Paleta
- body: `system-ui` font, bg `#F4F7FC`, cor `#2C3E50`
- `.card`: bg `#FFFFFF`, radius `12px`, padding `24px`, `max-width:360px`, `width:90%`
- `<h1>`: `1.3rem`, cor `#B2CEfE`
- `.label`: cor `#7A8BA3`, `0.85rem`
- `.info`: cor `#8FA0B8`, `0.8rem`

## On/Off Client
- `.valor-status`: `4rem`, transição cor `.3s`
- `.valor-status.on`: cor `#2E7D32`
- `.valor-status.off`: cor `#8FA0B8`
- Botões: radius `10px`, padding `0.65rem 1.25rem`, `0.9rem`, bold, cor `#fff`
- `.btn-on`: bg `#2E7D32`
- `.btn-off`: bg `#C62828`
- `.btn-accent`: bg `#B2CEfE`

## Sensor Client (DHT11)
- `.valor-sensor`: `2.5rem`, cor `#B2CEfE`

## Padrão JavaScript (todos os clients)
- `fetchState()` a cada 3s via `setInterval`
- Uptime format: `Xd Xh Xm Xs`
- Exibe IP, RSSI, uptime no `.info`
- Tratamento de erro: `catch{}` silencioso ou fallback

## Estrutura HTML Mínima
```html
<div class="card">
  <h1>Nome do Dispositivo</h1>
  <div class="valor-xxx" id="status">—</div>
  <div class="label">...</div>
  <div class="buttons">...</div>
  <div class="info" id="info"></div>
</div>
```
