# Linear Design System

> Aplicado ao ESP32 Bridge Dashboard — dark theme inspirado no Linear.app.

## Cores

```css
--bg: #010102            /* canvas fundo */
--surface: #0f1011       /* surface-1 cartões */
--surface-2: #141516     /* surface-2 device list */
--text: #f7f8f8          /* ink texto principal */
--muted: #d0d6e0         /* ink-muted texto secundário */
--muted-subtle: #8a8f98  /* ink-subtle labels */
--primary: #5e6ad2       /* lavender-blue destaque */
--primary-strong: #828fff
--border: #23252a        /* hairline */
--success: #27a644
--danger: #e5484d
```

## Tipografia

- Font: **Inter** (fallback system-ui)
- Display headings: weight 600, letter-spacing negativo
- Labels/eyebrow: weight 500, letter-spacing 0.4px, uppercase
- Body: weight 400

## Cantos

- Cartões: `12px` (lg)
- Hero panel: `16px` (xl)
- Botões: `8px` (md)
- Badges/pills: `9999px`

## Elevação

Linear usa ladder de superfícies (canvas → surface-1 → surface-2) + bordas hairline. Sem sombras.
