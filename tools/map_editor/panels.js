const preferenceKey = 'stickmon-map-inspector-panels-v1';

export function openPanel(id) {
  const panel = document.getElementById(id);
  if (panel) panel.open = true;
}

export function initPanels() {
  const panels = [...document.querySelectorAll('details[data-inspector-panel]')];
  let saved = {};
  try {
    const value = JSON.parse(localStorage.getItem(preferenceKey) || '{}');
    if (value && typeof value === 'object' && !Array.isArray(value)) saved = value;
  } catch (error) {
    console.warn('无法读取面板展开偏好，使用默认状态', error);
  }
  for (const panel of panels) {
    if (typeof saved[panel.id] === 'boolean') panel.open = saved[panel.id];
    panel.addEventListener('toggle', () => {
      saved[panel.id] = panel.open;
      try { localStorage.setItem(preferenceKey, JSON.stringify(saved)); }
      catch (error) { console.warn('无法保存面板展开偏好，本次仍可正常使用', error); }
    });
  }
  const button = document.getElementById('mapSizeHelp');
  const help = document.getElementById('mapSizeHelpContent');
  button.addEventListener('click', event => {
    event.preventDefault();
    event.stopPropagation();
    help.togglePopover();
    if (help.matches(':popover-open')) {
      const rect = button.getBoundingClientRect();
      help.style.top = `${Math.max(12, Math.min(rect.bottom + 8, innerHeight - help.offsetHeight - 12))}px`;
      help.style.left = `${Math.max(12, Math.min(rect.right - help.offsetWidth, innerWidth - help.offsetWidth - 12))}px`;
    }
  });
  help.addEventListener('toggle', () => button.setAttribute('aria-expanded', String(help.matches(':popover-open'))));
}
