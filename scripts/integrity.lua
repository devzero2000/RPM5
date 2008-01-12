-----BEGIN PGP SIGNED MESSAGE-----
Hash: SHA1

integrity.version = "0.9.0"

function integrity.validate(ctx, cfg)
    --  display configuration specification information
    if rpm.verbose() then
        io.stderr:write("rpm: integrity.validate: ctx.rpm.name = \""    .. ctx.rpm.name    .. "\"\n")
        io.stderr:write("rpm: integrity.validate: ctx.rpm.mode = \""    .. ctx.rpm.mode    .. "\"\n")
        io.stderr:write("rpm: integrity.validate: cfg.Id = \""          .. cfg.Id          .. "\"\n")
        io.stderr:write("rpm: integrity.validate: cfg.Name = \""        .. cfg.Name        .. "\"\n")
        io.stderr:write("rpm: integrity.validate: cfg.Version = \""     .. cfg.Version     .. "\"\n")
        io.stderr:write("rpm: integrity.validate: cfg.Issued = \""      .. cfg.Issued      .. "\"\n")
        io.stderr:write("rpm: integrity.validate: cfg.Issuer = \""      .. cfg.Issuer      .. "\"\n")
        io.stderr:write("rpm: integrity.validate: cfg.Description = \"" .. cfg.Description .. "\"\n")
        io.stderr:write("rpm: integrity.validate: cfg.Package = \""     .. cfg.Package     .. "\"\n")
    end

    --  process "Package" constraints
    if cfg.Package ~= nil then
        --  query RPMDB for names of all installed packages
        local packages = rpm.query("%{NAME}", true, "*")
        --  iterate over all constraints
        for _, constraint in ipairs(util.rsplit(util.rsubst(cfg.Package, "(?s)^\\s*(.+?)\\s*$", "%1"), "(?s)\\s+")) do
            --  parse constraint
            local s, _, m = util.rmatch(constraint, "(?s)^(!?)([^:]+):(!?)(.+)$")
            if s == nil then
                return "ERROR: Invalid syntax in \"Package\" constraint: \"" .. constraint .. "\"."
            end
            local mode_negate    = m[1] ~= ""
            local mode_regex     = m[2]
            local package_negate = m[3] ~= ""
            local package_regex  = m[4]
            --  apply the mode filter
            local mode_matches, _, _ = util.rmatch(ctx.rpm.mode, mode_regex);
            if     (not mode_negate and mode_matches ~= nil)
                or (    mode_negate and mode_matches == nil) then
                --  apply the package filter to names of all installed packages
                for _, package in ipairs(packages) do
                    local package_matches, _, _ = util.rmatch(package, package_regex)
                    if  not (   (not package_negate and package_matches ~= nil)
                             or (    package_negate and package_matches == nil)) then
                        --  indicate integrity validation error
                        return
                            "ERROR: Installed package \"" .. package .. "\" " ..
                            "not covered by \"Package\" constraint \"" .. package_regex .. "\" " ..
                            "under RPM mode \"" .. ctx.rpm.mode .. "\""
                    end
                end
            end
        end
    end

    --  indicate integrity validation success
    return "OK"
end

-----BEGIN PGP SIGNATURE-----
Version: GnuPG v2.0.8 (OpenPKG-CURRENT)

iEYEARECAAYFAkeIus4ACgkQ4NtEALXQTms37ACdGutltMufb5o7ow9a+i9BWlWQ
ABUAoIzBInaquk+Rl5dZ6UPsNF8L9sxl
=6qWp
-----END PGP SIGNATURE-----
