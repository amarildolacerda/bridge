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
  --bg: #010102;           /* canvas fundo da página */
  --surface: #0f1011;      /* surface-1: cartões */
  --surface-2: #141516;    /* surface-2: device list item bg */
  --text: #f7f8f8;         /* ink: texto principal */
  --muted: #d0d6e0;        /* ink-muted: texto secundário */
  --muted-subtle: #8a8f98; /* ink-subtle: labels, meta */
  --primary: #5e6ad2;      /* lavender-blue destaque */
  --primary-strong: #828fff;
  --primary-focus: #5e69d1;
  --border: #23252a;       /* hairline */
  --border-strong: #34343a;
  --success: #27a644;      /* verde online/ligado */
  --danger: #e5484d;       /* vermelho offline/desligado */
}
```

> Linear-style dark theme. Sem sombras — hierarquia via surface ladder + hairline borders.

## Componentes

### Cartão (`.card`)
- bg `var(--surface)`, border `1px solid var(--border)`, border-radius `12px`
- padding `16px`
- Título `<h2>` cor `var(--primary)`, tamanho `0.95rem`, weight `600`

### Hero
- Flex row, gap `16px`
- `.hero-card`: flexível, padding `18px 20px`
- `.eyebrow`: `13px`, letter-spacing `0.4px`, uppercase, weight `500`, cor `var(--primary)`
- `<h1>`: `1.5rem`, weight `600`, letter-spacing `-0.6px`

### Summary Grid (`.summary-grid`)
- 3 colunas iguais, gap `12px`
- `.metric`: surface + border + radius `12px`, padding `14px 16px`
- `.metric-label`: `12px`, uppercase, letter-spacing `0.4px`, weight `500`, cor `var(--muted-subtle)`
- `.metric-value`: `1.02rem`, weight `600`, cor `var(--text)`

### Content Grid (`.content`)
- 2 colunas: `minmax(280px, 340px)` (sidebar) + `minmax(0, 1fr)` (main)
- gap `16px`

### Row (`.row`)
- Flex space-between, padding `10px 0`, border-bottom `1px solid var(--border)`
- `.label`: cor `var(--muted-subtle)`
- `.value`: weight `600`, cor `var(--text)`

### Badge (`.badge`)
- Inline-flex, min-width `72px`, padding `2px 8px`, border-radius `9999px`
- `12px`, weight `500`
- `.badge.on`: bg green `rgba(39, 166, 68, 0.15)`, cor `var(--success)`
- `.badge.off`: bg red `rgba(229, 72, 77, 0.15)`, cor `var(--danger)`

### Device List Item (`.device`)
- bg `var(--surface-2)`, border `1px solid var(--border)`, border-radius `12px`
- padding `14px`
- `.device-head`: flex row, gap `8px`, wrap
- `.dev-name`: weight `600`
- `.dev-meta`: `0.76rem`, cor `var(--muted-subtle)`, margin `6px 0 8px`
- `.dev-state`: `0.9rem`, cor `var(--muted)`

### LED Indicator (`.led`)
- `9px` círculo, box-shadow `0 0 0 4px rgba(94, 106, 210, 0.12)`
- `.led.on`: bg `var(--success)`
- `.led.off`: bg `#3e3e44`

### Botões
- `.btn`: surface + border `1px solid var(--primary)`, radius `8px`, padding `10px 16px`
- `0.85rem`, weight `500`, cor `var(--primary)`
- Hover: bg `var(--primary)`, cor `#fff`
- `.btn-danger`: border/cor `var(--danger)`, hover bg `var(--danger)`
- `.btn:disabled`: opacity `0.5`, cursor not-allowed

### Copy Button (`.cpy`)
- bg `var(--primary)`, cor `#fff`, radius `6px`, padding `3px 10px`
- `12px`, weight `500`, uppercase, cursor pointer

### Empty State (`.empty`)
- Text-align center, padding `36px 16px`, cor `var(--muted-subtle)`

### Code (`.code`)
- Monospace stack (`JetBrains Mono`, `SF Mono`), `0.82rem`, bg `var(--surface-2)`, padding `3px 8px`, radius `6px`

## Background do Body
```css
body {
  background: var(--bg);  /* #010102 — flat, sem gradientes */
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
