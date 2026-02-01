#!/bin/sh

make run_fresh
dot -Tpng ir_plan.dot -o ir_plan.png
dot -Tpng ir_plan_mat.dot -o ir_plan_mat.png
dot -Tpng ir_plan_mat_num.dot -o ir_plan_mat_num.png
dot -Tpng api_plan.dot -o api_plan.png