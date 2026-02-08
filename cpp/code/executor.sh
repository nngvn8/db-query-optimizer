#!/bin/sh

make run_fresh
dot -Tpng ir_plan.dot -o ir_plan.png
dot -Tpng ir_plan_semi_j.dot -o ir_plan_semi_j.png
dot -Tpng ir_plan_agg_opt.dot -o ir_plan_agg_opt.png
dot -Tpng ir_plan_mat.dot -o ir_plan_mat.png
dot -Tpng ir_plan_mat_num.dot -o ir_plan_mat_num.png
dot -Tpng api_plan.dot -o api_plan.png

dot -Tpng ir_plan_l.dot -o ir_plan_l.png
dot -Tpng ir_plan_semi_j_l.dot -o ir_plan_semi_j_l.png
dot -Tpng ir_plan_agg_opt_l.dot -o ir_plan_agg_opt_l.png
dot -Tpng ir_plan_mat_l.dot -o ir_plan_mat_l.png
dot -Tpng ir_plan_mat_num_l.dot -o ir_plan_mat_num_l.png
dot -Tpng api_plan_l.dot -o api_plan_l.png