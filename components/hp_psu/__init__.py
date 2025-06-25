import esphome.codegen as cg

CODEOWNERS = ["@neiderhauser"]
DEPENDENCIES = ["i2c"]

hp_psu_ns = cg.esphome_ns.namespace("hp_psu")

CONF_HP_PSU_ID = "hp_psu_id"
