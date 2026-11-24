-- Pandoc Lua filter for the IncuNest user manual (Word output).
-- Maps the Markdown callouts to the styles defined in reference.docx:
--   ::: warning / caution / note  ->  "Warning Box" / "Caution Box" / "Note Box"
--   [PENDING: ...]{.pending}       ->  "Pending" character style
-- For HTML output the classes are kept and styled by manual.css.

local box_styles = {
  warning = "Warning Box",
  caution = "Caution Box",
  note = "Note Box",
  ["draft-banner"] = "Warning Box",
  ["guide-intro"] = "Guide Intro",
  ["guide-end"] = "Guide End",
}

function Div(el)
  if not FORMAT:match("docx") then return nil end
  for _, cls in ipairs(el.classes) do
    local style = box_styles[cls]
    if style then
      el.attributes["custom-style"] = style
      return el
    end
  end
  return nil
end

function Span(el)
  if not FORMAT:match("docx") then return nil end
  if el.classes:includes("pending") then
    el.attributes["custom-style"] = "Pending"
    return el
  end
  return nil
end
