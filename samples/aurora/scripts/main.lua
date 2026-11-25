-- Aurora sample script. The only callback of the M3S slice: scale the parallax input
-- with the "parallax_strength" setting (0 = still, 1 = full movement).
function on_parallax(x, y)
  local k = settings.parallax_strength
  return x * k, y * k
end
